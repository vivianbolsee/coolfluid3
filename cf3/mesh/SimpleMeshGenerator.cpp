// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "common/Builder.hpp"
#include "common/OptionArray.hpp"
#include "common/OptionURI.hpp"
#include "common/OptionList.hpp"
#include "common/OptionT.hpp"
#include "common/FindComponents.hpp"
#include "common/Foreach.hpp"
#include "common/PE/Comm.hpp"
#include "common/PE/debug.hpp"
#include "common/Log.hpp"
#include "common/Core.hpp"

#include "mesh/Mesh.hpp"
#include "mesh/Connectivity.hpp"
#include "mesh/MergedParallelDistribution.hpp"
#include "mesh/ParallelDistribution.hpp"
#include "mesh/SimpleMeshGenerator.hpp"
#include "mesh/Region.hpp"
#include "mesh/Dictionary.hpp"
#include "mesh/Space.hpp"
#include "mesh/MeshElements.hpp"
#include "mesh/Cells.hpp"
#include "mesh/Faces.hpp"
#include "mesh/Elements.hpp"
#include "mesh/Field.hpp"

namespace cf3 {
namespace mesh {

using namespace common;
using namespace common::XML;

////////////////////////////////////////////////////////////////////////////////

ComponentBuilder < SimpleMeshGenerator, MeshGenerator, LibMesh > SimpleMeshGenerator_Builder;

////////////////////////////////////////////////////////////////////////////////

SimpleMeshGenerator::SimpleMeshGenerator ( const std::string& name  ) :
  MeshGenerator ( name )
{
  mark_basic();

  options().add_option("nb_cells", m_nb_cells)
      .description("Vector of number of cells in each direction")
      .pretty_name("Number of Cells")
      .link_to(&m_nb_cells)
      .mark_basic();

  options().add_option("offsets", m_offsets)
      .description("Vector of offsets in direction")
      .pretty_name("Offsets")
      .link_to(&m_offsets)
      .mark_basic();

  options().add_option("lengths", m_lengths)
      .description("Vector of lengths each direction")
      .pretty_name("Lengths")
      .link_to(&m_lengths)
      .mark_basic();

  options().add_option("part", PE::Comm::instance().rank())
      .description("Part number (e.g. rank of processors)")
      .pretty_name("Part");

  options().add_option("nb_parts", PE::Comm::instance().size())
      .description("Total number of partitions (e.g. number of processors)")
      .pretty_name("Number of Partitions");

  options().add_option("bdry", true)
      .description("Generate Boundary")
      .pretty_name("Boundary");
}

////////////////////////////////////////////////////////////////////////////////

SimpleMeshGenerator::~SimpleMeshGenerator()
{
}

//////////////////////////////////////////////////////////////////////////////

void SimpleMeshGenerator::execute()
{
  if ( is_null(m_mesh) )
    throw SetupError(FromHere(), "Mesh URI not set");

  if (m_nb_cells.size() == 1 && m_lengths.size() == 1)
  {
    create_line();
  }
  else if (m_nb_cells.size() == 2  && m_lengths.size() == 2)
  {
    create_rectangle();
  }
  else
  {
    throw SetupError(FromHere(), "Invalid size of the vector number of cells. Only 1D and 2D supported now.");
  }
  m_mesh->raise_mesh_loaded();
}

////////////////////////////////////////////////////////////////////////////////

void SimpleMeshGenerator::create_line()
{
  Mesh& mesh = *m_mesh;
  const Uint x_segments = m_nb_cells[XX];
  const Real x_len = m_lengths[XX];
  const Real x_offset = (m_offsets.empty() ? 0. : m_offsets[XX]);
  const Uint nb_parts = options().option("nb_parts").value<Uint>();
  const bool bdry = options().option("bdry").value<bool>();


  Uint part = options().option("part").value<Uint>();
  enum HashType { NODES=0, ELEMS=1 };
  // Create a hash
  boost::shared_ptr<MergedParallelDistribution> tmp_hash = allocate_component<MergedParallelDistribution>("tmp_hash");
  MergedParallelDistribution& hash = *tmp_hash;

  std::vector<Uint> num_obj(2);
  num_obj[NODES] = x_segments+1;
  num_obj[ELEMS] = x_segments;
  hash.options().configure_option("nb_obj",num_obj);
  hash.options().configure_option("nb_parts",nb_parts);

  // find ghost nodes
  std::map<Uint,Uint> ghost_nodes_loc;
  Uint glb_node_idx;
  for(Uint i = 0; i < x_segments; ++i)
  {
    if (hash.subhash(ELEMS).part_owns(part,i))
    {
      glb_node_idx = i;
      if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
      {
        ghost_nodes_loc[glb_node_idx]=0; // this value will be set further
      }

      glb_node_idx = (i+1);
      if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
      {
        ghost_nodes_loc[glb_node_idx]=0; // this value will be set further
      }
    }
  }

  mesh.initialize_nodes(hash.subhash(NODES).nb_objects_in_part(part)+ghost_nodes_loc.size(), DIM_1D);
  Region& region = mesh.topology().create_region("interior");
  Dictionary& nodes = mesh.geometry_fields();

  Uint glb_node_start_idx = hash.subhash(NODES).start_idx_in_part(part);

  const Real x_step = x_len / static_cast<Real>(x_segments);
  for(Uint i = 0; i <= x_segments; ++i)
  {
    glb_node_idx = i;

    if (hash.subhash(NODES).part_owns(part,glb_node_idx))
    {
      cf3_assert(glb_node_idx-glb_node_start_idx < nodes.size());
      common::Table<Real>::Row row = nodes.coordinates()[glb_node_idx-glb_node_start_idx];
      row[XX] = static_cast<Real>(i) * x_step + x_offset;
      nodes.rank()[glb_node_idx-glb_node_start_idx]=part;
      nodes.glb_idx()[glb_node_idx-glb_node_start_idx]=glb_node_idx;
    }
  }

  // add ghost nodes
  Uint glb_ghost_node_start_idx = hash.subhash(NODES).nb_objects_in_part(part);
  Uint loc_node_idx = glb_ghost_node_start_idx;
  cf3_assert(glb_ghost_node_start_idx <= nodes.size());
  foreach_container((const Uint glb_ghost_node_idx) (Uint& loc_ghost_node_idx),ghost_nodes_loc)
  {
    Uint i = glb_ghost_node_idx;
    loc_ghost_node_idx = loc_node_idx++;
    cf3_assert(loc_ghost_node_idx < nodes.size());
    common::Table<Real>::Row row = nodes.coordinates()[loc_ghost_node_idx];
    row[XX] = static_cast<Real>(i) * x_step + x_offset;
    nodes.rank()[loc_ghost_node_idx]=hash.subhash(NODES).proc_of_obj(glb_ghost_node_idx);
    nodes.glb_idx()[loc_ghost_node_idx]=glb_ghost_node_idx;
  }
  Handle<Cells> cells = region.create_component<Cells>("Line");
  cells->initialize("cf3.mesh.LagrangeP1.Line1D",nodes);
  cells->resize(hash.subhash(ELEMS).nb_objects_in_part(part));
  Connectivity& connectivity = cells->geometry_space().connectivity();
  common::List<Uint>& elem_rank = cells->rank();
  common::List<Uint>& elem_glb_idx = cells->glb_idx();

  Uint glb_elem_start_idx = hash.subhash(ELEMS).start_idx_in_part(part);
  Uint glb_elem_idx;
  for(Uint i = 0; i < x_segments; ++i)
  {
    glb_elem_idx = i;

    if (hash.subhash(ELEMS).part_owns(part,glb_elem_idx))
    {
      Connectivity::Row nodes = connectivity[glb_elem_idx-glb_elem_start_idx];
      elem_rank[glb_elem_idx-glb_elem_start_idx] = part;
      elem_glb_idx[glb_elem_idx-glb_elem_start_idx] = glb_elem_idx;

      glb_node_idx = i;
      if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
        nodes[0] = ghost_nodes_loc[glb_node_idx];
      else
        nodes[0] = glb_node_idx-glb_node_start_idx;

      glb_node_idx = (i+1);
      if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
        nodes[1] = ghost_nodes_loc[glb_node_idx];
      else
        nodes[1] = glb_node_idx-glb_node_start_idx;
    }
  }

  if (bdry)
  {
    Uint point_node;

    // xneg
    {
      glb_elem_idx = x_segments;
      Handle<Faces> xneg = mesh.topology().create_region("xneg").create_component<Faces>("Point");
      xneg->initialize("cf3.mesh.LagrangeP0.Point1D", nodes);
      if (hash.subhash(ELEMS).part_owns(part,0u))
      {
        xneg->resize(1);
        glb_node_idx=0u;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          point_node = ghost_nodes_loc[glb_node_idx];
        else
          point_node = glb_node_idx-glb_node_start_idx;

        xneg->geometry_space().connectivity()[0][0]=point_node;
        xneg->rank()[0]=part;
        xneg->glb_idx()[0]=glb_elem_idx;
      }
    }

    // xpos
    {
      glb_elem_idx = x_segments+1;
      Handle<Faces> xpos = mesh.topology().create_region("xpos").create_component<Faces>("Point");
      xpos->initialize("cf3.mesh.LagrangeP0.Point1D", nodes);
      if (hash.subhash(ELEMS).part_owns(part,x_segments-1))
      {
        xpos->resize(1);

        glb_node_idx = x_segments;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          point_node = ghost_nodes_loc[glb_node_idx];
        else
          point_node = glb_node_idx-glb_node_start_idx;

        xpos->geometry_space().connectivity()[0][0]=point_node;
        xpos->rank()[0]=part;
        xpos->glb_idx()[0]=glb_elem_idx;
      }
    }
  }

}

////////////////////////////////////////////////////////////////////////////////

void SimpleMeshGenerator::create_rectangle()
{
  Mesh& mesh = *m_mesh;
  const Uint x_segments = m_nb_cells[XX];
  const Uint y_segments = m_nb_cells[YY];
  const Real x_len = m_lengths[XX];
  const Real y_len = m_lengths[YY];
  const Real x_offset = (m_offsets.empty() ? 0. : m_offsets[XX]);
  const Real y_offset = (m_offsets.empty() ? 0. : m_offsets[YY]);
  const Uint nb_parts = options().option("nb_parts").value<Uint>();
  const bool bdry = options().option("bdry").value<bool>();


  Uint part = options().option("part").value<Uint>();
  enum HashType { NODES=0, ELEMS=1 };
  // Create a hash
  boost::shared_ptr<MergedParallelDistribution> tmp_hash = allocate_component<MergedParallelDistribution>("tmp_hash");
  MergedParallelDistribution& hash = *tmp_hash;
  std::vector<Uint> num_obj(2);
  num_obj[NODES] = (x_segments+1)*(y_segments+1);
  num_obj[ELEMS] = x_segments*y_segments;
  hash.options().configure_option("nb_obj",num_obj);
  hash.options().configure_option("nb_parts",nb_parts);

  Region& region = mesh.topology().create_region("interior");
  Dictionary& nodes = mesh.geometry_fields();


  // find ghost nodes
  std::map<Uint,Uint> ghost_nodes_loc;
  Uint glb_node_idx;
  for(Uint j = 0; j < y_segments; ++j)
  {
    for(Uint i = 0; i < x_segments; ++i)
    {
      if (hash.subhash(ELEMS).part_owns(part,i+j*x_segments))
      {
        glb_node_idx = j * (x_segments+1) + i;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
        {
          ghost_nodes_loc[glb_node_idx]=0; // this value will be set further
        }

        glb_node_idx = j * (x_segments+1) + (i+1);
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
        {
          ghost_nodes_loc[glb_node_idx]=0; // this value will be set further
        }

        glb_node_idx = (j+1) * (x_segments+1) + i;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
        {
          ghost_nodes_loc[glb_node_idx]=0; // this value will be set further
        }

        glb_node_idx = (j+1) * (x_segments+1) + (i+1);
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
        {
          ghost_nodes_loc[glb_node_idx]=0; // this value will be set further
        }
      }
    }
  }

  mesh.initialize_nodes(hash.subhash(NODES).nb_objects_in_part(part)+ghost_nodes_loc.size(), DIM_2D);
  Uint glb_node_start_idx = hash.subhash(NODES).start_idx_in_part(part);

  const Real x_step = x_len / static_cast<Real>(x_segments);
  const Real y_step = y_len / static_cast<Real>(y_segments);
  Real y;
  for(Uint j = 0; j <= y_segments; ++j)
  {
    y = static_cast<Real>(j) * y_step;
    for(Uint i = 0; i <= x_segments; ++i)
    {
      glb_node_idx = j * (x_segments+1) + i;

      if (hash.subhash(NODES).part_owns(part,glb_node_idx))
      {
        cf3_assert(glb_node_idx-glb_node_start_idx < nodes.size());
        common::Table<Real>::Row row = nodes.coordinates()[glb_node_idx-glb_node_start_idx];
        row[XX] = static_cast<Real>(i) * x_step + x_offset;
        row[YY] = y + y_offset;
        nodes.rank()[glb_node_idx-glb_node_start_idx]=part;
        nodes.glb_idx()[glb_node_idx-glb_node_start_idx]=glb_node_idx;
      }
    }
  }

  // add ghost nodes
  Uint glb_ghost_node_start_idx = hash.subhash(NODES).nb_objects_in_part(part);
  Uint loc_node_idx = glb_ghost_node_start_idx;
  cf3_assert(glb_ghost_node_start_idx <= nodes.size());
  foreach_container((const Uint glb_ghost_node_idx) (Uint& loc_ghost_node_idx),ghost_nodes_loc)
  {
    Uint j = glb_ghost_node_idx / (x_segments+1);
    Uint i = glb_ghost_node_idx - j*(x_segments+1);
    loc_ghost_node_idx = loc_node_idx++;
    cf3_assert(loc_ghost_node_idx < nodes.size());
    common::Table<Real>::Row row = nodes.coordinates()[loc_ghost_node_idx];
    row[XX] = static_cast<Real>(i) * x_step + x_offset;
    row[YY] = static_cast<Real>(j) * y_step + y_offset;
    nodes.rank()[loc_ghost_node_idx]=hash.subhash(NODES).proc_of_obj(glb_ghost_node_idx);
    nodes.glb_idx()[loc_ghost_node_idx]=glb_ghost_node_idx;
  }
  Handle<Cells> cells = region.create_component<Cells>("Quad");
  cells->initialize("cf3.mesh.LagrangeP1.Quad2D",nodes);

  cells->resize(hash.subhash(ELEMS).nb_objects_in_part(part));
  Connectivity& connectivity = cells->geometry_space().connectivity();
  common::List<Uint>& elem_rank = cells->rank();
  common::List<Uint>& elem_glb_idx = cells->glb_idx();

  Uint glb_elem_start_idx = hash.subhash(ELEMS).start_idx_in_part(part);
  Uint glb_elem_idx;
  for(Uint j = 0; j < y_segments; ++j)
  {
    for(Uint i = 0; i < x_segments; ++i)
    {
      glb_elem_idx = j * x_segments + i;

      if (hash.subhash(ELEMS).part_owns(part,glb_elem_idx))
      {
        Connectivity::Row nodes = connectivity[glb_elem_idx-glb_elem_start_idx];
        elem_rank[glb_elem_idx-glb_elem_start_idx] = part;
        elem_glb_idx[glb_elem_idx-glb_elem_start_idx] = glb_elem_idx;

        glb_node_idx = j * (x_segments+1) + i;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          nodes[0] = ghost_nodes_loc[glb_node_idx];
        else
          nodes[0] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = j * (x_segments+1) + (i+1);
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          nodes[1] = ghost_nodes_loc[glb_node_idx];
        else
          nodes[1] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = (j+1) * (x_segments+1) + i;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          nodes[3] = ghost_nodes_loc[glb_node_idx];
        else
          nodes[3] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = (j+1) * (x_segments+1) + (i+1);
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          nodes[2] = ghost_nodes_loc[glb_node_idx];
        else
          nodes[2] = glb_node_idx-glb_node_start_idx;
      }

    }
  }


  if (bdry)
  {
    glb_elem_idx =  (x_segments+1) * (y_segments+1);
    std::vector<Uint> line_nodes(2);
    Handle<Faces> left = mesh.topology().create_region("left").create_component<Faces>("Line");
    left->initialize("cf3.mesh.LagrangeP1.Line2D", nodes);
    Connectivity::Buffer left_connectivity = left->geometry_space().connectivity().create_buffer();
    common::List<Uint>::Buffer left_rank = left->rank().create_buffer();
    common::List<Uint>::Buffer left_glb_idx = left->glb_idx().create_buffer();
    for(Uint j = 0; j < y_segments; ++j)
    {
      if (hash.subhash(ELEMS).part_owns(part,j*x_segments))
      {
        glb_node_idx = j * (x_segments+1);
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[0] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[0] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = (j+1) * (x_segments+1);
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[1] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[1] = glb_node_idx-glb_node_start_idx;

        left_connectivity.add_row(line_nodes);
        left_rank.add_row(part);
        left_glb_idx.add_row(glb_elem_idx);
      }
      ++glb_elem_idx;
    }

    Handle<Faces> right = mesh.topology().create_region("right").create_component<Faces>("Line");
    right->initialize("cf3.mesh.LagrangeP1.Line2D", nodes);
    Connectivity::Buffer right_connectivity = right->geometry_space().connectivity().create_buffer();
    common::List<Uint>::Buffer right_rank = right->rank().create_buffer();
    common::List<Uint>::Buffer right_glb_idx = right->glb_idx().create_buffer();

    for(Uint j = 0; j < y_segments; ++j)
    {
      if (hash.subhash(ELEMS).part_owns(part,j*x_segments+x_segments-1))
      {
        glb_node_idx = j * (x_segments+1) + x_segments;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[1] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[1] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = (j+1) * (x_segments+1) + x_segments;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[0] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[0] = glb_node_idx-glb_node_start_idx;

        right_connectivity.add_row(line_nodes);
        right_rank.add_row(part);
        right_glb_idx.add_row(glb_elem_idx);

      }
      ++glb_elem_idx;
    }

    Handle<Faces> bottom = mesh.topology().create_region("bottom").create_component<Faces>("Line");
    bottom->initialize("cf3.mesh.LagrangeP1.Line2D", nodes);
    Connectivity::Buffer bottom_connectivity = bottom->geometry_space().connectivity().create_buffer();
    common::List<Uint>::Buffer bottom_rank = bottom->rank().create_buffer();
    common::List<Uint>::Buffer bottom_glb_idx = bottom->glb_idx().create_buffer();

    for(Uint i = 0; i < x_segments; ++i)
    {
      if (hash.subhash(ELEMS).part_owns(part,i))
      {
        glb_node_idx = i;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[0] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[0] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = i+1;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[1] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[1] = glb_node_idx-glb_node_start_idx;

        bottom_connectivity.add_row(line_nodes);
        bottom_rank.add_row(part);
        bottom_glb_idx.add_row(glb_elem_idx);
      }
      ++glb_elem_idx;
    }

    Handle<Faces> top = mesh.topology().create_region("top").create_component<Faces>("Line");
    top->initialize("cf3.mesh.LagrangeP1.Line2D", nodes);
    Connectivity::Buffer top_connectivity = top->geometry_space().connectivity().create_buffer();
    common::List<Uint>::Buffer top_rank = top->rank().create_buffer();
    common::List<Uint>::Buffer top_glb_idx = top->glb_idx().create_buffer();

    for(Uint i = 0; i < x_segments; ++i)
    {
      if (hash.subhash(ELEMS).part_owns(part,y_segments*(x_segments)+i))
      {
        glb_node_idx = y_segments * (x_segments+1) + i;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[1] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[1] = glb_node_idx-glb_node_start_idx;

        glb_node_idx = y_segments * (x_segments+1) + i+1;
        if (hash.subhash(NODES).part_owns(part,glb_node_idx) == false)
          line_nodes[0] = ghost_nodes_loc[glb_node_idx];
        else
          line_nodes[0] = glb_node_idx-glb_node_start_idx;

        top_connectivity.add_row(line_nodes);
        top_rank.add_row(part);
        top_glb_idx.add_row(glb_elem_idx);
      }
      ++glb_elem_idx;
    }
  }

  // sanity checks

  boost_foreach(Elements& elements, find_components_recursively<Elements>(mesh.topology()))
  {
    cf3_assert_desc(elements.uri().string() + " ( "+to_str(elements.size())+"!="+to_str(elements.rank().size()),elements.size() == elements.rank().size());
    boost_foreach(Uint r, elements.rank().array())
    {
      cf3_assert( r == part);
    }
  }

  cf3_assert(nodes.rank().size() == nodes.size());
}

////////////////////////////////////////////////////////////////////////////////

} // mesh
} // cf3
