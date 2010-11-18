// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <boost/filesystem/path.hpp>

#include "Common/OptionT.hpp"
#include "Common/URI.hpp"

namespace CF {
namespace Common {

////////////////////////////////////////////////////////////////////////////////

template < typename TYPE>
OptionT<TYPE>::OptionT ( const std::string& name, const std::string& desc, value_type def) :
    Option(name, desc, def)
{
//    CFinfo
//        << " creating OptionT [" << m_name << "]"
//        << " of type [" << m_type << "]"
//        << " w default [" << def_str() << "]"
//        << " w desc [" << m_description << "]\n"
//        << CFendl;
  m_restricted_list.push_back(def);
}

template < typename TYPE>
void OptionT<TYPE>::configure ( XmlNode& node )
{
  TYPE val;
  const char * type_str = XmlTag<TYPE>::type();
  XmlNode * type_node = node.first_node(type_str);

  if(type_node != nullptr)
    to_value(*type_node,val);
  else
  {
    std::string str;
    throw XmlError(FromHere(), std::string("Could not find a value of this type [") + type_str + "].");
  }
  m_value = val;
  copy_to_linked_params(val);
}

template < typename TYPE >
void OptionT<TYPE>::copy_to_linked_params ( const TYPE& val )
{
  BOOST_FOREACH ( void* v, this->m_linked_params )
  {
    TYPE* cv = static_cast<TYPE*>(v);
    *cv = val;
  }
}

template < typename TYPE >
const char* OptionT<TYPE>::tag () const
{
  return XmlTag<TYPE>::type();
}

////////////////////////////////////////////////////////////////////////////////

/// explicit instantiation to avoid missing symbols in certain compilers
template class OptionT< bool >;
template class OptionT< int >;
template class OptionT< std::string >;
template class OptionT< CF::Uint >;
template class OptionT< CF::Real >;
template class OptionT< CF::Common::URI >;
template class OptionT< boost::filesystem::path >;

////////////////////////////////////////////////////////////////////////////////

} // Common
} // CF