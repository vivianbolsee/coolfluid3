// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "common/Builder.hpp"

#include "Diffusion2D.hpp"

namespace cf3 {
namespace physics {
namespace Scalar {

////////////////////////////////////////////////////////////////////////////////////

common::ComponentBuilder < Scalar::Diffusion2D,
                           physics::Variables,
                           LibScalar >
                           Builder_Diffusion2D;

Diffusion2D::Diffusion2D(const std::string& name) : VariablesT<Diffusion2D>(name)
{
  description().set_variables("U",MODEL::_ndim);
}

Diffusion2D::~Diffusion2D() {}

////////////////////////////////////////////////////////////////////////////////////

} // Scalar
} // physics
} // cf3
