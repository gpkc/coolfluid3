// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "boost/assign/list_of.hpp"

#include "Common/Signal.hpp"
#include "Common/CBuilder.hpp"
#include "Common/OptionT.hpp"
 

#include "Common/XML/SignalOptions.hpp"

#include "Mesh/CMeshReader.hpp"
#include "Mesh/CDomain.hpp"

#include "Solver/CModelSteady.hpp"
#include "Solver/CPhysicalModel.hpp"
#include "Solver/CSolver.hpp"

#include "RDM/Core/LinearAdv2D.hpp"       // supported physics
#include "RDM/Core/LinearAdv3D.hpp"       // supported physics
#include "RDM/Core/RotationAdv2D.hpp"     // supported physics
#include "RDM/Core/Burgers2D.hpp"         // supported physics
#include "RDM/Core/LinearAdvSys2D.hpp"    // supported physics
#include "RDM/Core/Euler2D.hpp"           // supported physics

#include "RDM/Core/SteadyExplicit.hpp"

namespace CF {
namespace RDM {

using namespace CF::Common;
using namespace CF::Common::XML;
using namespace CF::Mesh;
using namespace CF::Solver;

Common::ComponentBuilder < SteadyExplicit, Solver::CWizard, LibCore > SteadyExplicit_Builder;

////////////////////////////////////////////////////////////////////////////////

SteadyExplicit::SteadyExplicit ( const std::string& name  ) :
  Solver::CWizard ( name )
{
  // signals

  this->regist_signal ( "create_model" , "Creates a scalar advection model", "Create Model" )->signal->connect ( boost::bind ( &SteadyExplicit::signal_create_model, this, _1 ) );

  signal("create_component")->is_hidden = true;
  signal("rename_component")->is_hidden = true;
  signal("delete_component")->is_hidden = true;
  signal("move_component")->is_hidden   = true;

  signal("create_model")->signature->connect( boost::bind( &SteadyExplicit::signature_create_model, this, _1));
}

////////////////////////////////////////////////////////////////////////////////

SteadyExplicit::~SteadyExplicit()
{
}

////////////////////////////////////////////////////////////////////////////////

void SteadyExplicit::signal_create_model ( Common::SignalArgs& node )
{
  SignalOptions options( node );

  std::string name  = options.option<std::string>("ModelName");

  CModel::Ptr model = Core::instance().root().create_component_ptr<CModelSteady>( name );

  // create the Physical Model
  CPhysicalModel::Ptr pm = model->create_component_ptr<CPhysicalModel>("Physics");
  pm->mark_basic();

  std::string phys  = options.option<std::string>("PhysicalModel");

  pm->configure_property( "Type", phys );

  Uint neqs = 0;
  Uint ndim = 0;

  if( phys == "LinearAdv2D")
  {
    neqs = LinearAdv2D::neqs;
    ndim = LinearAdv2D::ndim;
  }
  if( phys == "LinearAdv3D")
  {
    neqs = LinearAdv3D::neqs;
    ndim = LinearAdv3D::ndim;
  }
  if( phys == "Burgers2D")
  {
    neqs = Burgers2D::neqs;
    ndim = Burgers2D::ndim;
  }
  if( phys == "RotationAdv2D")
  {
    neqs = RotationAdv2D::neqs;
    ndim = RotationAdv2D::ndim;
  }
  if( phys == "LinearAdvSys2D")
  {
    neqs = LinearAdvSys2D::neqs;
    ndim = LinearAdvSys2D::ndim;
  }
  if( phys == "Euler2D")
  {
    neqs = Euler2D::neqs;
    ndim = Euler2D::ndim;
  }

  if ( (neqs == 0) || (ndim == 0) )
    throw SetupError( FromHere(), "Unsupported physics type : " + phys );

  pm->configure_property( "DOFs", neqs );

  pm->configure_property( "Dimensions", ndim );

  model->create_domain( "Domain" );

  // setup iterative solver
  CSolver::Ptr solver = build_component_abstract_type<CSolver>( LibCore::library_namespace() + ".RKRD", "Solver");
  solver->mark_basic();
  model->add_component( solver );

  solver->configure_property("physics", pm->uri() );
}

////////////////////////////////////////////////////////////////////////////////

void SteadyExplicit::signature_create_model( SignalArgs& node )
{
  SignalOptions options( node );

  options.add<std::string>("ModelName", std::string(), "Name for created model" );

  std::vector<std::string> models = boost::assign::list_of
      ( LinearAdv2D::type_name() )
      ( LinearAdv3D::type_name() )
      ( RotationAdv2D::type_name() )
      ( Euler2D::type_name() )
      ( LinearAdvSys2D::type_name() )
      ( Burgers2D::type_name() ) ;

  options.add<std::string>("PhysicalModel", std::string(), "Name of the Physical Model", models, " ; ");

}

////////////////////////////////////////////////////////////////////////////////

} // RDM
} // CF