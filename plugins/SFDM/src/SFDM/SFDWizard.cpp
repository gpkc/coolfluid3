// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "Common/Log.hpp"
#include "Common/CBuilder.hpp"
#include "Common/FindComponents.hpp"
#include "Common/Foreach.hpp"
#include "Common/CBuilder.hpp"
#include "Common/OptionT.hpp"
#include "Common/OptionURI.hpp"
#include "Common/CGroupActions.hpp"
#include "Common/Signal.hpp"

#include "Common/XML/SignalOptions.hpp"

#include "Solver/CModel.hpp"
#include "Solver/FlowSolver.hpp"
#include "Solver/CPhysicalModel.hpp"
#include "Solver/CTime.hpp"

#include "Solver/Actions/CAdvanceTime.hpp"
#include "Solver/Actions/CCriterionTime.hpp"
#include "Solver/Actions/CForAllCells.hpp"

#include "Mesh/CDomain.hpp"
#include "Mesh/CMesh.hpp"
#include "Mesh/CElements.hpp"
#include "Mesh/CSpace.hpp"
#include "Mesh/ElementType.hpp"
#include "Mesh/CRegion.hpp"

#include "Mesh/Actions/CInitFieldConstant.hpp"
#include "Mesh/Actions/CInitFieldFunction.hpp"
#include "Mesh/Actions/CBuildFaces.hpp"
#include "Mesh/Actions/CBuildVolume.hpp"
#include "Mesh/Actions/CreateSpaceP0.hpp"

#include "SFDM/SFDWizard.hpp"
#include "SFDM/UpdateSolution.hpp"
#include "SFDM/ComputeUpdateCoefficient.hpp"
#include "SFDM/OutputIterationInfo.hpp"
#include "SFDM/ComputeRhsInCell.hpp"
#include "SFDM/CreateSpace.hpp"
#include "SFDM/CreateSFDFields.hpp"

//////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace SFDM {

  using namespace Common;
  using namespace Common::XML;

  using namespace Mesh;
  using namespace Solver;
  using namespace Solver::Actions;
  using namespace Mesh::Actions;

////////////////////////////////////////////////////////////////////////////////

Common::ComponentBuilder < SFDWizard, CWizard, LibSFDM> SFDWizard_Builder;

//////////////////////////////////////////////////////////////////////////////

SFDWizard::SFDWizard( const std::string& name )
: CWizard(name)
{

  properties()["brief"] = std::string("MISSING");
  properties()["description"] = std::string("MISSING");

  properties().add_option( OptionT<std::string>::create("model","Model Name","Name to give to the simulation model","SFD_simulation") )->mark_basic();
  properties().add_option( OptionT<Uint>::create("dim","Dimension","Dimension of the simulation",1u) )->mark_basic();
  properties().add_option( OptionT<std::string>::create("physics","Physics","Builder name for the physical model","CF.Physics.") )->mark_basic();
  properties().add_option( OptionT<Uint>::create("P","Polynomial Order","The order of the polynomial of the solution",0u) )->mark_basic();
  properties().add_option( OptionT<Uint>::create("RK_stages","Runge Kutta stages","The number of Runge Kutta stages",2u) )->mark_basic();
  properties().add_option( OptionT<Real>::create(FlowSolver::Tags::cfl(),"CFL","The Courant-Friedrichs-Lax Number",1.) )->mark_basic();
  properties().add_option( OptionT<bool>::create(FlowSolver::Tags::time_accurate(),"Time Accurate","Time accurate or steady state",true) )->mark_basic();
  properties().add_option( OptionT<bool>::create("output_file","Output File","File to write","mesh_t${time}.msh") )->mark_basic();

  SignalPtr sig_create_simulation = regist_signal ( "create_simulation" , "Create a simulation", "Create Simulation" );
    sig_create_simulation->signal    -> connect ( boost::bind ( &SFDWizard::signal_create_simulation, this, _1 ) );
    sig_create_simulation->signature -> connect ( boost::bind ( &SFDWizard::signature_create_simulation, this, _1) );

  SignalPtr sig_prepare_simulation = regist_signal ( "prepare_simulation" , "Prepare a simulation", "Prepare Simulation" );
    sig_prepare_simulation->signal    -> connect ( boost::bind ( &SFDWizard::signal_prepare_simulation, this, _1 ) );
    sig_prepare_simulation->signature -> connect ( boost::bind ( &SFDWizard::signature_prepare_simulation, this, _1) );

  SignalPtr sig_init_sol = regist_signal ( "initialize_solution" , "Initialize Solution", "Initialize Solution" );
    sig_init_sol->signal    -> connect ( boost::bind ( &SFDWizard::signal_initialize_solution, this, _1 ) );
    sig_init_sol->signature -> connect ( boost::bind ( &SFDWizard::signature_initialize_solution, this, _1) );

  SignalPtr sig_start_simulation = regist_signal ( "start_simulation" , "Start a simulation", "Start Simulation" );
    sig_start_simulation->signal    -> connect ( boost::bind ( &SFDWizard::signal_start_simulation, this, _1 ) );
    sig_start_simulation->signature -> connect ( boost::bind ( &SFDWizard::signature_start_simulation, this, _1) );

  m_model_link = create_static_component_ptr<CLink>("current_model");
}

/////////////////////////////////////////////////////////////////////////////

CModel& SFDWizard::model()
{
  return m_model_link->follow()->as_type<CModel>();
}

/////////////////////////////////////////////////////////////////////////////

void SFDWizard::create_simulation()
{
  CModel& model = Core::instance().root().create_component<CModel>(property("model").value_str());
  m_model_link->link_to(model);

  CPhysicalModel& physical_model = model.create_physics("Physics");

  /// @todo should be setup differently
  physical_model.configure_property("solution_state",std::string("CF.AdvectionDiffusion.State"+property("dim").value_str()+"D"));

  CDomain& domain                = model.create_domain("Domain");
  CTime& time                    = model.create_time("Time");
  CSolver& solver                = model.create_solver("CF.Solver.FlowSolver");

  // These 2 functions are the only specific ones to SFDM (together with some configuration options)
  // -------------------------
  build_solve();
  build_setup();
  // -------------------------

  solver.configure_property(FlowSolver::Tags::physical_model(),physical_model.uri());
  solver.configure_property(FlowSolver::Tags::time(),time.uri());

  model.tools().create_component_ptr<CInitFieldFunction>("initialize_solution");

  CFinfo << "\nCreate the mesh in ["<<domain.uri().path()<<"] and call \""<<uri().path()<<"/prepare\"" << CFendl;
}

void SFDWizard::prepare_simulation()
{
  if (m_model_link->is_linked() == false)
    throw SetupError(FromHere(),"Model was not created through this wizard");

  CModel& model = m_model_link->follow()->as_type<CModel>();

  std::vector<CMesh::Ptr> meshes = find_components<CMesh>(model.domain()).as_vector();
  if (meshes.size() == 0)
    throw SetupError(FromHere(),"Mesh was not added to the domain");

  if (meshes.size() > 1)
  {
    if (properties().check("mesh"))
    {
      model.solver().configure_property("mesh",property("mesh").value<URI>());
    }
    else
    {
      properties().add_option( OptionURI::create("mesh","Mesh","The mesh to solve on",model.domain().uri()/URI("mesh")) )->mark_basic();
      throw SetupError(FromHere(),"Multiple meshes exist in ["+model.domain().uri().path()+"]\n"
                       "Set the option \"mesh\" to specify the path to the mesh");
    }
  }
  else
  {
    model.solver().configure_property("mesh",meshes[0]->uri());
  }

  CFinfo << CFendl;
  CFinfo << "To add Boundary Conditions:   " << "call " << model.solver().uri().path() << "/create_bc_action" << CFendl;
  CFinfo << "To add Inner domain actions:  " << "call " << model.solver().uri().path() << "/create_inner_action" << CFendl;
  CFinfo << "To initialize solution:       " << model.tools().get_child("initialize_solution").uri().path() << "     (needs configuring) " << CFendl;
  CFinfo << "                    or:       " << "call " << uri().path() << "/initialize_solution" << CFendl;
  CFinfo << CFendl;
  CFinfo << "To start simulation:          " << "call " << uri().path() << "/start_simulation" << CFendl;

}

//////////////////////////////////////////////////////////////////////////////

void SFDWizard::start_simulation(const Real& end_time, const Real& time_step)
{
  if (m_model_link->is_linked() == false)
    throw SetupError(FromHere(),"Model was not created through this wizard");

  CModel& model = m_model_link->follow()->as_type<CModel>();

  model.time()
      .configure_property("end_time",end_time)
      .configure_property("time_step",time_step);

  model.solver().configure_option_recursively(FlowSolver::Tags::cfl(),property(FlowSolver::Tags::cfl()).value<Real>());
  model.solver().configure_option_recursively(FlowSolver::Tags::time_accurate(),property(FlowSolver::Tags::time_accurate()).value<bool>());

  model.simulate();
}

//////////////////////////////////////////////////////////////////////////////

void SFDWizard::initialize_solution(const std::vector<std::string>& functions)
{
  CAction& init_solution = model().tools().get_child("initialize_solution").as_type<CAction>();
  init_solution.configure_property("functions",functions);
  init_solution.execute();
}


//////////////////////////////////////////////////////////////////////////////

void SFDWizard::signal_create_simulation( SignalArgs& node )
{
  create_simulation();
}

////////////////////////////////////////////////////////////////////////////////

void SFDWizard::signature_create_simulation( SignalArgs& node )
{
}

//////////////////////////////////////////////////////////////////////////////

void SFDWizard::signal_prepare_simulation( SignalArgs& node )
{
  prepare_simulation();
}

////////////////////////////////////////////////////////////////////////////////

void SFDWizard::signature_prepare_simulation( SignalArgs& node )
{
}

//////////////////////////////////////////////////////////////////////////////

void SFDWizard::signal_initialize_solution( SignalArgs& node )
{
  SignalOptions options( node );

  std::vector<std::string> functions =
      options.array<std::string>("functions");
  initialize_solution(functions);
}

////////////////////////////////////////////////////////////////////////////////

void SFDWizard::signature_initialize_solution( SignalArgs& node )
{
  SignalOptions options( node );

  options.add("functions", std::vector<std::string>() , "Analytical functions (x,y,z) ", " ; " );
}

//////////////////////////////////////////////////////////////////////////////

void SFDWizard::signal_start_simulation( SignalArgs& node )
{
  SignalOptions options( node );

  if (options.exists("current_time"))
    m_model_link->follow()->as_type<CModel>().configure_property("time",options.option<Real>("time"));

  Real end_time = 0;
  if (options.exists("end_time"))
    end_time = options.option<Real>("end_time");

  if (options.exists("time_step"))
  {
    const Real time_step = options.option<Real>("time_step");
    start_simulation(end_time,time_step);
  }
  else
  {
    start_simulation(end_time);
  }
}

////////////////////////////////////////////////////////////////////////////////

void SFDWizard::signature_start_simulation( SignalArgs& node )
{
  SignalOptions options( node );

  // name
  CTime& time = m_model_link->follow()->as_type<CModel>().time();
  options.add<Real>("current_time", time.time() , "End Time" );
  options.add<Real>("end_time", time.property("end_time").value<Real>() , "End Time" );
  options.add<Real>("time_step", time.property("time_step").value<Real>() , "End Time" );
}

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Here starts the solver itself. This could be customized later for
// different algorithms. Could be another wizard.
//////////////////////////////////////////////////////////////////////////////

void SFDWizard::build_solve()
{
  FlowSolver& solver = m_model_link->follow()->as_type<CModel>().solver().as_type<FlowSolver>();

  Component& iterate = solver.create_solve("iterate","CF.Solver.Actions.CIterate");
  Component& RK = iterate.create_component("1_RK_stages","CF.RungeKutta.RK");
  RK.configure_property("stages",property("RK_stages").value<Uint>());
  Component& compute_rhs = RK.access_component("1_for_each_stage/1_pre_update_actions").create_component<CGroupActions>("1_compute_rhs").mark_basic();
  compute_rhs.add_tag(FlowSolver::Tags::inner());
  compute_rhs.create_component <CInitFieldConstant>("1.1_init_residual")
    .mark_basic()
    .configure_property("constant",0.)
    .property("field").add_tag(FlowSolver::Tags::residual());

  compute_rhs.create_static_component<CInitFieldConstant>("1.2_init_wave_speed")
    .mark_basic()
    .configure_property("constant",0.)
    .property("field").add_tag(FlowSolver::Tags::wave_speed());

  Component& for_all_cells =
    compute_rhs.create_component<CForAllCells>("1.3_for_all_cells").mark_basic();
  Component& compute_rhs_in_cell = for_all_cells.create_component<ComputeRhsInCell>("1.3.1_compute_rhs_in_cell").mark_basic();

  RK.access_component("1_for_each_stage/1_pre_update_actions").create_component<ComputeUpdateCoefficient>("2_compute_update_coeff").mark_basic();
  iterate.create_component<OutputIterationInfo>("2_output_info").mark_basic();
  iterate.create_component<CCriterionTime>("time_stop_criterion").mark_basic();

  /// @todo configure differently
  solver.configure_option_recursively("riemann_solver",std::string("CF.RiemannSolvers.Roe"));
  solver.configure_option_recursively("roe_state",std::string("CF.AdvectionDiffusion.State"+property("dim").value_str()+"D"));

  solver.configure_option_recursively("solution_state",m_model_link->follow()->as_type<CModel>().physics().solution_state().uri());

}

//////////////////////////////////////////////////////////////////////////////

void SFDWizard::build_setup()
{
  /// Create a Solver::Action that gets executed automatically when the FlowSolver
  /// has been configured with ALL of the following:
  /// - physical_model
  /// - mesh
  /// - time

  FlowSolver& solver = model().solver().as_type<FlowSolver>();
  CAction& setup = solver.as_type<FlowSolver>().create_setup(FlowSolver::Tags::setup(),"CF.SFDM.SFDSetup");

  /// Create a mesh transformer to adapt the mesh for SFDM
  CMeshTransformer& transform_mesh = setup.create_component<CMeshTransformer>("1_transform_mesh").mark_basic().as_type<CMeshTransformer>();
  transform_mesh.create_component<CBuildFaces>       ("1_build_faces").mark_basic().configure_property("store_cell2face",true);
  transform_mesh.create_component<CreateSpaceP0>     ("2_create_space_P0").mark_basic();
  transform_mesh.create_component<SFDM::CreateSpace> ("3_create_sfd_spaces").mark_basic().configure_property("P",property("P").value<Uint>());
  transform_mesh.create_component<CBuildVolume>      ("4_build_volume_field").mark_basic();

  /// Create an action that creates all fields used for SFDM
  setup.create_component<CreateSFDFields>("2_create_sfd_fields");
}

//////////////////////////////////////////////////////////////////////////////

ComponentBuilder<SFDSetup,CAction,LibSFDM> SFDSetup_builder;

void SFDSetup::execute()
{
  /// This gets executed AUTOMATICALLY when the FlowSolver has been configured with ALL of the following:
  /// - physical_model
  /// - mesh
  /// - time

  /// 1) execute all mesh transforming actions, and field creation (added in SFDWizard::build_setup())
  boost_foreach(CAction& action, find_components<CAction>(*this))
    action.execute();

  /// @todo configure this differently perhaps
  /// 2) set looping regions to the entire mesh
  access_component("../iterate/1_RK_stages/1_for_each_stage/1_pre_update_actions/1_compute_rhs/1.3_for_all_cells").configure_property("regions",std::vector<URI>(1,mesh().topology().uri()));

  /// 3) configure the initialize_solution component. The field must be set to the solution.
  access_component("../../tools/initialize_solution").configure_property("field",mesh().get_child(FlowSolver::Tags::solution()).uri());
}

} // SFDM
} // CF