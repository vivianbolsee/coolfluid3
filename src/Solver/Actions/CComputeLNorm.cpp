// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <cmath>

#include "Common/Log.hpp"

#include "Common/CBuilder.hpp"
#include "Common/OptionT.hpp"
#include "Common/OptionComponent.hpp"
#include "Common/Foreach.hpp"

#include "Mesh/CField2.hpp"
#include "Mesh/CTable.hpp"

#include "Solver/Actions/CComputeLNorm.hpp"

/////////////////////////////////////////////////////////////////////////////////////

using namespace CF::Common;
using namespace CF::Mesh;

namespace CF {
namespace Solver {
namespace Actions {

///////////////////////////////////////////////////////////////////////////////////////

Common::ComponentBuilder < CComputeLNorm, CAction, LibActions > CComputeLNorm_Builder;

///////////////////////////////////////////////////////////////////////////////////////
  
CComputeLNorm::CComputeLNorm ( const std::string& name ) : CAction(name)
{
  mark_basic();

  // properties

  m_properties.add_property("Norm", Real(0.) );

  // options

  m_properties.add_option< OptionT<bool> >("Scale", "Scales (divides) the norm by the number of entries (ignored if order zero)", true);
  m_properties.add_option< OptionT<Uint> >("Order", "Order of the p-norm, zero if L-inf", 2u);

  m_properties.add_option(OptionComponent<CField2>::create("Field", "Field for which to compute the norm", &m_field));
}

////////////////////////////////////////////////////////////////////////////////

void CComputeLNorm::execute()
{
  if (m_field.expired()) throw SetupError(FromHere(), "Field was not set");

  CTable<Real>& table = m_field.lock()->data();
  CTable<Real>::ArrayT& array =  table.array();

  const Uint nbrows = table.size();

  if ( !nbrows ) throw SetupError(FromHere(), "Field has empty table");

  Real norm = 0.;

  const Uint order = m_properties.option("Order").value<Uint>();
  switch(order)
  {
  case 2: // L2
    boost_foreach(CTable<Real>::ConstRow row, array )
        norm += row[0]*row[0];
    norm = std::sqrt(norm);
    break;
  case 1: // L1
    boost_foreach(CTable<Real>::ConstRow row, array )
        norm += std::abs( row[0] );
    break;
  case 0: // treat as Linf
    boost_foreach(CTable<Real>::ConstRow row, array )
        norm += std::max( std::abs(row[0]), norm );
    break;
  default: // Lp
    boost_foreach(CTable<Real>::ConstRow row, array )
        norm += std::abs( pow(row[0], order) );
    norm = std::pow(norm, 1./order );
    break;
  }

  if( m_properties.option("Scale").value<bool>() && order )
    norm /= nbrows;

  properties().property("Norm").change_value(norm);
}

////////////////////////////////////////////////////////////////////////////////

} // Actions
} // Solver
} // CF

////////////////////////////////////////////////////////////////////////////////////

