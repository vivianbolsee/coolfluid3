// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.


#ifndef CF_Solver_Actions_Proto_ElementMatrix_hpp
#define CF_Solver_Actions_Proto_ElementMatrix_hpp

// Default maximum number of different element matrices that can appear in an expression
#ifndef CF_PROTO_MAX_ELEMENT_MATRICES
  #define CF_PROTO_MAX_ELEMENT_MATRICES 5
#endif

#include<iostream>

#include <boost/fusion/adapted/mpl.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/fusion/container/vector/convert.hpp>

#include <boost/mpl/max.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector_c.hpp>

#include <boost/proto/core.hpp>

#include "Math/MatrixTypes.hpp"

#include "Terminals.hpp"
#include "Transforms.hpp"

/// @file
/// System matrix block accumultation. Current prototype uses dense a dense Eigen matrix and is purely for proof-of-concept

namespace CF {
namespace Solver {
namespace Actions {
namespace Proto {

/// Represents an element matrix
template<typename T>
struct ElementMatrix : T
{
};

/// Some predefined element matrices (more can be user-defined, but you have to change the number in the MPL int_ so the type is long and tedious)
static boost::proto::terminal< ElementMatrix< boost::mpl::int_<0> > >::type const _A = {};
static boost::proto::terminal< ElementMatrix< boost::mpl::int_<1> > >::type const _T = {};

/// Reperesents element RHS vector
struct ElementRHS
{
};

/// Terminal for the element RHS vector ("b")
static boost::proto::terminal<ElementRHS>::type const _b = {};

/// Transform to set a range with true indicating a variable that has an equation, and false for no equation
template<int I>
struct IsEquationVariable :
  boost::proto::or_
  <
    boost::proto::when // scalar field used as equation in an element matrix
    <
      boost::proto::function<boost::proto::terminal< ElementMatrix<boost::proto::_> >, boost::proto::terminal< Var<boost::mpl::int_<I>, boost::proto::_> > >,
      boost::mpl::true_()
    >,
    boost::proto::when
    <
      boost::proto::terminal<boost::proto::_>,
      boost::mpl::false_()
    >,
    boost::proto::when
    <
      boost::proto::nary_expr<boost::proto::_, boost::proto::vararg<boost::proto::_> >
    , boost::proto::fold
      <
        boost::proto::_, boost::mpl::false_(), boost::mpl::max< boost::proto::_state, boost::proto::call< IsEquationVariable<I> > >()
      >
    >
  >
{
};

/// Wrap IsEquationVariable as a lambda expression
template<typename I, typename ExprT>
struct EquationVariablesOp
{
  typedef typename boost::result_of<IsEquationVariable<I::value>(ExprT)>::type type;
};

/// Calculate if a variable is part of an equation
template<typename ExprT, typename NbVarsT>
struct EquationVariables
{
  /// Types of the used variables
  typedef typename boost::fusion::result_of::as_vector
  <
    typename boost::mpl::transform
    <
      typename boost::mpl::copy<boost::mpl::range_c<int,0,NbVarsT::value>, boost::mpl::back_inserter< boost::mpl::vector_c<Uint> > >::type, //range from 0 to NbVarsT
      EquationVariablesOp<boost::mpl::_1, ExprT>
    >::type
  >::type type;
};

/// Get the width of a field varaible, based on the variable type
template<typename T, typename SF>
struct FieldWidth
{
  static const Uint value = 0;
};

/// Scalars have width 1
template<typename SF>
struct FieldWidth<ScalarField, SF>
{
  static const Uint value = 1;
};

/// VectorFields have the same dimension as the problem domain
template<typename SF>
struct FieldWidth<VectorField, SF>
{
  static const Uint value = SF::dimension;
};

/// Given a variable's data, get the product of the number of nodes with the dimension variable (i.e. the size of the element matrix if this variable would be the only one in the problem)
template<typename VariableT, typename SF>
struct NodesTimesDim
{
  typedef boost::mpl::int_<SF::nb_nodes * FieldWidth<VariableT, SF>::value> type;
};

template<typename SF>
struct NodesTimesDim<boost::mpl::void_, SF>
{
  typedef boost::mpl::int_<0> type;
};

/// The size of the element matrix for each variable
template<typename VariablesT, typename VariablesSFT>
struct MatrixSizePerVar
{
  typedef typename boost::mpl::transform
  <
    typename boost::mpl::copy<VariablesT, boost::mpl::back_inserter< boost::mpl::vector0<> > >::type,
    VariablesSFT,
    NodesTimesDim<boost::mpl::_1, boost::mpl::_2>
  >::type type;
};

/// Filter the matrix siz so equation variables are the only ones left with non-zero size
template<typename MatrixSizesT, typename EquationVariablesT>
struct FilterMatrixSizes
{
  typedef typename boost::mpl::transform
  <
    MatrixSizesT,
    typename boost::mpl::copy<EquationVariablesT, boost::mpl::back_inserter< boost::mpl::vector0<> > >::type,
    boost::mpl::if_<boost::mpl::_2, boost::mpl::_1, boost::mpl::int_<0> >
  >::type type;
};

/// Calculate the size of the element matrix
template<typename MatrixSizesT, typename EquationVariablesT>
struct ElementMatrixSize
{
  typedef typename boost::mpl::fold
  <
    typename FilterMatrixSizes<MatrixSizesT, EquationVariablesT>::type,
    boost::mpl::int_<0>,
    boost::mpl::plus<boost::mpl::_1, boost::mpl::_2>
  >::type type;
};

/// Get the offset in the element matrix for an equation variable
template<typename MatrixSizesT, typename EquationVariablesT, typename VarIdxT>
struct VarOffset
{
  typedef typename FilterMatrixSizes<MatrixSizesT, EquationVariablesT>::type filtered_sizes;
  typedef typename boost::mpl::advance<typename boost::mpl::begin<filtered_sizes>::type, VarIdxT>::type last;
  
  typedef typename boost::mpl::fold
  <
    boost::mpl::iterator_range<typename boost::mpl::begin<filtered_sizes>::type, last>,
    boost::mpl::int_<0>,
    boost::mpl::plus<boost::mpl::_1, boost::mpl::_2>
  >::type type;
};

/// Get a given element matrix
struct ElementMatrixValue :
  boost::proto::transform<ElementMatrixValue>
{
 
  template<typename ExprT, typename StateT, typename DataT>
  struct impl : boost::proto::transform_impl<ExprT, StateT, DataT>
  {
    typedef typename boost::remove_reference<DataT>::type::ElementMatrixT& result_type;

    result_type operator ()(typename impl::expr_param, typename impl::state_param state, typename impl::data_param data) const
    {
      return data.element_matrix(state);
    }
  };
};

/// Only get the rows relevant for a given variable
struct ElementMatrixRowsValue :
  boost::proto::transform<ElementMatrixRowsValue>
{
 
  template<typename ExprT, typename StateT, typename DataT>
  struct impl : boost::proto::transform_impl<ExprT, StateT, DataT>
  {
    typedef typename VarDataType<ExprT, DataT>::type VarDataT;
    typedef Eigen::Block
    <
      typename boost::remove_reference<DataT>::type::ElementMatrixT,
      VarDataT::dimension * VarDataT::SF::nb_nodes,
      VarDataT::matrix_size
    > result_type;
    
    result_type operator ()(typename impl::expr_param, typename impl::state_param state, typename impl::data_param data) const
    {
      return data.element_matrix(state).template block<VarDataT::dimension * VarDataT::SF::nb_nodes, VarDataT::matrix_size>(VarDataT::offset, 0);
    }
  };
};

/// Only get a block containing the rows for the first var and the cols for the second
struct ElementMatrixBlockValue :
  boost::proto::transform<ElementMatrixBlockValue>
{
 
  template<typename ExprT, typename StateT, typename DataT>
  struct impl : boost::proto::transform_impl<ExprT, StateT, DataT>
  {
    typedef typename VarDataType<typename boost::proto::result_of::value<typename boost::proto::result_of::child_c<ExprT, 1>::type>::type, DataT>::type RowVarDataT;
    typedef typename VarDataType<typename boost::proto::result_of::value<typename boost::proto::result_of::child_c<ExprT, 2>::type>::type, DataT>::type ColVarDataT;
    
    typedef Eigen::Block
    <
      typename boost::remove_reference<DataT>::type::ElementMatrixT,
      RowVarDataT::dimension * RowVarDataT::SF::nb_nodes,
      ColVarDataT::dimension * ColVarDataT::SF::nb_nodes
    > result_type;
    
    result_type operator ()(typename impl::expr_param, typename impl::state_param state, typename impl::data_param data) const
    {
      return data.element_matrix(state).template block
      <
        RowVarDataT::dimension * RowVarDataT::SF::nb_nodes,
        ColVarDataT::dimension * ColVarDataT::SF::nb_nodes
      >(RowVarDataT::offset, ColVarDataT::offset);
    }
  };
};

/// Get the RHS vector
struct ElementRHSValue :
  boost::proto::transform<ElementRHSValue>
{
  template<typename ExprT, typename StateT, typename DataT>
  struct impl : boost::proto::transform_impl<ExprT, StateT, DataT>
  {
    typedef typename boost::remove_reference<DataT>::type::ElementVectorT& result_type;

    result_type operator ()(typename impl::expr_param, typename impl::state_param, typename impl::data_param data) const
    {
      return data.element_rhs();
    }
  };
};

template<typename GrammarT>
struct ElementMatrixGrammar :
  boost::proto::or_
  <
    boost::proto::when
    <
      boost::proto::terminal< ElementMatrix<boost::proto::_> >,
      ElementMatrixValue(boost::proto::_expr, boost::proto::_value)
    >,
    boost::proto::when
    <
      boost::proto::function<boost::proto::terminal< ElementMatrix<boost::proto::_> >, FieldTypes>,
      ElementMatrixRowsValue(boost::proto::_value(boost::proto::_child1), boost::proto::_value(boost::proto::_child0))
    >,
    boost::proto::when
    <
      boost::proto::function<boost::proto::terminal< ElementMatrix<boost::proto::_> >, FieldTypes, FieldTypes>,
      ElementMatrixBlockValue(boost::proto::_expr, boost::proto::_value(boost::proto::_child0))
    >,
    boost::proto::when
    <
      boost::proto::terminal<ElementRHS>,
      ElementRHSValue
    >
  >
{
};


} // namespace Proto
} // namespace Actions
} // namespace Solver
} // namespace CF

#endif // CF_Solver_Actions_Proto_ElementMatrix_hpp