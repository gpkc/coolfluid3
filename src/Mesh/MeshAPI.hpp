#ifndef CF_MeshAPI_HH
#define CF_MeshAPI_HH

////////////////////////////////////////////////////////////////////////////////

#include "Common/ExportAPI.hpp"
#include "Common/ModuleRegister.hpp"

////////////////////////////////////////////////////////////////////////////////

/// Define the macro Mesh_API
/// @note build system defines Mesh_EXPORTS when compiling MeshTools files
#ifdef Mesh_EXPORTS
#   define Mesh_API      CF_EXPORT_API
#   define Mesh_TEMPLATE
#else
#   define Mesh_API      CF_IMPORT_API
#   define Mesh_TEMPLATE CF_TEMPLATE_EXTERN
#endif

////////////////////////////////////////////////////////////////////////////////

namespace CF {

  /// Basic Classes for Mesh applications used by CF
  namespace Mesh {

////////////////////////////////////////////////////////////////////////////////
    
  /// Class defines the initialization and termination of the library Mesh
  /// @author Tiago Quintino
  class MeshLib :
      public Common::ModuleRegister<MeshLib>
  {
  public:

    /// Static function that returns the module name.
    /// Must be implemented for the ModuleRegister template
    /// @return name of the module
    static std::string getModuleName() { return "Mesh"; }

    /// Static function that returns the description of the module.
    /// Must be implemented for the ModuleRegister template
    /// @return descripton of the module
    static std::string getModuleDescription()
    {
      return "This library implements the mesh manipulation API.";
    }

    /// Gets the Class name
    static std::string getClassName() { return "MeshLib"; }

    /// Start profiling
    virtual void initiate();

    /// Stop profiling
    virtual void terminate();

  }; // end MeshLib

////////////////////////////////////////////////////////////////////////////////

} // namespace Mesh
} // namespace CF

////////////////////////////////////////////////////////////////////////////////

#endif // CF_Mesh_HH
