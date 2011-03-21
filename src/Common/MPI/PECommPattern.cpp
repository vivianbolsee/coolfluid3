// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

////////////////////////////////////////////////////////////////////////////////

#include "Common/LibCommon.hpp"
#include "Common/CBuilder.hpp"
#include "Common/Log.hpp"
#include "Common/MPI/PE.hpp"
#include "Common/MPI/PECommPattern.hpp"
#include "Common/MPI/PEObjectWrapper.hpp"

#include "Common/FindComponents.hpp"

/*
TODO:
1.: remove boost.mpi deps when pe ready
2.: if 1. is done, remove &xxx[0] accessses
*/

////////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Common  {

////////////////////////////////////////////////////////////////////////////////
// Provider
////////////////////////////////////////////////////////////////////////////////

Common::ComponentBuilder < PECommPattern, Component, LibCommon > PECommPattern_Provider;

////////////////////////////////////////////////////////////////////////////////
// Constructor & destructor
////////////////////////////////////////////////////////////////////////////////

PECommPattern::PECommPattern(const std::string& name): Component(name), m_gid(new PEObjectWrapperPtr<int>("dummy")),
  m_isUpdatable(0),
  m_add_buffer(0),
  m_mov_buffer(0),
  m_rem_buffer(0),
  m_sendCount(mpi::PE::instance().size(),0),
  m_sendMap(0),
  m_recvCount(mpi::PE::instance().size(),0),
  m_recvMap(0)
{
  //self->regist_signal ( "update" , "Executes communication patterns on all the registered data.", "" )->signal->connect ( boost::bind ( &CommPattern2::update, self, _1 ) );
  m_isUpToDate=false;
  m_isFreeze=false;
}

////////////////////////////////////////////////////////////////////////////////

PECommPattern::~PECommPattern()
{
  if (m_gid.get()!=nullptr) m_gid->remove_tag("gid_of_"+this->name());
}

////////////////////////////////////////////////////////////////////////////////
// Commpattern handling
////////////////////////////////////////////////////////////////////////////////

void PECommPattern::setup(PEObjectWrapper::Ptr gid, std::vector<Uint>& rank)
{
  // basic check
  BOOST_ASSERT( (Uint) gid->size()==rank.size() );
  if (gid->stride()!=1) throw CF::Common::BadValue(FromHere(),"Data to be registered as gid is not of stride=1.");
  if (gid->is_data_type_Uint()!=true) throw CF::Common::CastingFailed(FromHere(),"Data to be registered as gid is not of type Uint.");
  m_gid=gid;
  m_gid->add_tag("gid_of_"+this->name());
  if (get_child_ptr(gid->name()).get() == nullptr) add_component(gid);

  // sizesof datas matching
  BOOST_FOREACH( PEObjectWrapper& pobj, find_components_recursively<PEObjectWrapper>(*this) )
    if ((Uint) pobj.size()!=m_isUpdatable.size()+gid->size())
      throw CF::Common::BadValue(FromHere(),"Size does not match commpattern's size.");

  // add to add buffer
  // if performance issues, replace for(...) add(...) with direct push_back
  if (gid->size()!=0) {
    m_isUpToDate=false;
    std::vector<int> map(gid->size());
    for(int i=0; i<(int)map.size(); i++) map[i]=i;
    Uint *igid=(Uint*)gid->pack(map);
    std::vector<Uint>::iterator irank=rank.begin();
    for (Uint* iigid=igid;irank!=rank.end();irank++,iigid++)
      add(*iigid,*irank);
    delete[] igid;
/*
PECheckPoint(100,"-- Setup comission: --");
PEProcessSortedExecute(-1,
  BOOST_FOREACH(temp_buffer_item& i, m_add_buffer) std::cout << "("<< i.gid << "|" << i.rank << "|" << i.option << ")";
  std::cout << "\n" << std::flush;
)
*/
    setup();
  }
}

////////////////////////////////////////////////////////////////////////////////

// OK: how this works:
// 0.: ??? lock and pre-flush data ???
// 1.:

void PECommPattern::setup()
{
  // data management internal to this function
  // -----------------------------------------
  // ngid: number of updatable nodes of all ranks
  // gido: offset for easy navigation between processes in the global arrays
  // gidr: rank where node is updatable
/*
  // general constants
  const int nproc=mpi::PE::instance().size();
  const int irank=mpi::PE::instance().rank();

  // build ngid and gido
  std::vector<int> ngid(nproc,0);
  BOOST_FOREACH(bool is_updatable, m_updatable ) if (is_updatable) ngid[irank]++;
  ngid[irank]+=m_add_buffer.size();
//  boost::mpi::all_reduce(mpi::PE::instance(),&ngid[0],nproc,&ngid[0],boost::mpi::maximum<int>());
  int ntotalnodes=0;
  BOOST_FOREACH(int node,ngid) ntotalnodes+=node;
//  std::vector<int> ngid(nproc,0);
//  for(i=0;i<nproc;i++)
*/

//DEBUGVECTOR(ngid);
//DEBUGVECTOR(gida);

//--------------------------------------------------------------------

  // experimental version, supports one single setup call and only add

  // -- 1 -- data definition
/*
PECheckPoint(100,"-- step 1 --:");
*/

  // -- 2 -- get environment data
  // get stuff from environment
  const CPint irank=(CPint)mpi::PE::instance().rank();
  const CPint nproc=(CPint)mpi::PE::instance().size();
/*
PECheckPoint(100,"-- step 2 --:");
*/
  // -- 3 -- build receive info
  // build receive count and receive map, note that filtering out data from laying on current process
  BOOST_FOREACH(temp_buffer_item& i, m_add_buffer) m_recvCount[i.rank]++;
  m_recvCount[irank]=0;
  int recvSum=0;
  BOOST_FOREACH(CPint& i, m_recvCount) recvSum+=i;
  m_recvMap.reserve(recvSum);
  m_recvMap.assign(recvSum,std::numeric_limits<CPint>::max());
  std::vector<CPint> recvcounter(nproc,0);
  for (int i=1; i<nproc; i++) recvcounter[i]=m_recvCount[i-1]+recvcounter[i-1];
  CPint recvidx=0;
  BOOST_FOREACH(temp_buffer_item& i, m_add_buffer)
  {
    if (i.rank!=irank)
      m_recvMap[recvcounter[i.rank]++]=recvidx;
    recvidx++;
  }
  BOOST_FOREACH(CPint& i, m_recvMap)
    BOOST_ASSERT(i!=std::numeric_limits<CPint>::max());

/*
PECheckPoint(100,"-- step 3 --:");
PEProcessSortedExecute(-1,PEDebugVector(m_recvMap,m_recvMap.size()));
PEProcessSortedExecute(-1,PEDebugVector(m_recvCount,m_recvCount.size()));
*/

  // -- 4 -- building a distributed info for looking up communication
  // setting up m_isUpdatable and its counts (distributed over processes)
  // also computing displacements of counts
  m_isUpdatable.resize(m_add_buffer.size(),false);
  m_isUpdatable.reserve(m_add_buffer.size());
  std::vector<int> dist_nupdatable(nproc,0);
  for (int i=0; i<m_add_buffer.size(); i++)
    if (irank==m_add_buffer[i].rank)
    {
      dist_nupdatable[irank]++;
      m_isUpdatable[i]=true;
    }
  std::vector<int> dist_nupdatables(nproc);
  mpi::PE::instance().all_reduce(mpi::plus(),dist_nupdatable,dist_nupdatable);
  std::vector<int> dist_nupdatabledisp(nproc,0);
  for (int i=1; i<nproc; i++) dist_nupdatabledisp[i]=dist_nupdatable[i-1]+dist_nupdatabledisp[i-1];

PECheckPoint(100,"-- step 4 --:");
PEProcessSortedExecute(-1,PEDebugVector(dist_nupdatable,dist_nupdatable.size()));
PEProcessSortedExecute(-1,PEDebugVector(dist_nupdatabledisp,dist_nupdatabledisp.size()));

  // building info and communicating dist
  std::vector<dist_struct> dist(dist_nupdatable.size());
  CPint ilid=0;
  BOOST_FOREACH(dist_struct& i, dist)
  {
    i.rank=irank;
    i.lid=ilid;
    i.data=nullptr;
  }
/*
  std::vector<int> dist_nsend(nproc,0);


  CPint lid=0;
  BOOST_FOREACH(temp_buffer_item& i, m_add_buffer)
  {
    if (i.rank==irank)
      dist_lidupdatable.push_back(lid);
    lid++;
  }
  BOOST_ASSERT(dist_lidupdatable.size()==dist_nupdatable);
*/
/*
PECheckPoint(100,"XXXXX Distributed data, step 1:");
PEProcessSortedExecute(-1,PEDebugVector(dist_lidupdatable,dist_lidupdatable.size()));
PEProcessSortedExecute(-1,PEDebugVector(dist_rankupdatable,dist_rankupdatable.size()));
*/
}

////////////////////////////////////////////////////////////////////////////////

void PECommPattern::update()
{

}

////////////////////////////////////////////////////////////////////////////////

void PECommPattern::add(Uint gid, Uint rank)
{
  if (m_isFreeze) throw Common::ShouldNotBeHere(FromHere(),"Wanted to add nodes to commpattern '" + name() + "' which is freezed.");
  m_add_buffer.push_back(temp_buffer_item(gid,rank,false));
  m_isUpToDate=false;
}

////////////////////////////////////////////////////////////////////////////////

void PECommPattern::move(Uint gid, Uint rank, bool keep_as_ghost)
{
  if (m_isFreeze) throw Common::ShouldNotBeHere(FromHere(),"Wanted to moves nodes of commpattern '" + name() + "' which is freezed.");
  m_mov_buffer.push_back(temp_buffer_item(gid,rank,keep_as_ghost));
  m_isUpToDate=false;
}

////////////////////////////////////////////////////////////////////////////////

void PECommPattern::remove(Uint gid, Uint rank, bool on_all_ranks)
{
  if (m_isFreeze) throw Common::ShouldNotBeHere(FromHere(),"Wanted to delete nodes from commpattern '" + name() + "' which is freezed.");
  m_rem_buffer.push_back(temp_buffer_item(gid,rank,on_all_ranks));
  m_isUpToDate=false;
}

////////////////////////////////////////////////////////////////////////////////
// Component related
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

} // Common
} // CF











/*
#include <boost/lambda/lambda.hpp>

#include "Common/MPI/PE.hpp"
#include "Common/MPI/PECommPattern.hpp"
#include "Common/MPI/all_to_all.hpp"

////////////////////////////////////////////////////////////////////////////////

using namespace boost::lambda;

namespace CF {
  namespace Common  {

////////////////////////////////////////////////////////////////////////////////

PECommPattern::PECommPattern()
{
  m_isCommPatternPrepared=false;
  m_sendCount.resize(mpi::PE::instance().size(),0);
  m_sendMap.resize(0);
  m_receiveCount.resize(mpi::PE::instance().size(),0);
  m_receiveMap.resize(0);
  m_updatable.resize(0);
}

////////////////////////////////////////////////////////////////////////////////

PECommPattern::PECommPattern(std::vector<Uint> gid, std::vector<Uint> rank)
{
  m_isCommPatternPrepared=false;
  m_sendCount.resize(mpi::PE::instance().size(),0);
  m_sendMap.resize(0);
  m_receiveCount.resize(mpi::PE::instance().size(),0);
  m_receiveMap.resize(0);
  m_updatable.resize(0);
  setup(gid,rank);
}

////////////////////////////////////////////////////////////////////////////////

PECommPattern::~PECommPattern()
{
}

////////////////////////////////////////////////////////////////////////////////

void PECommPattern::setup(std::vector<Uint> gid, std::vector<Uint> rank)
{

  const Uint irank=mpi::PE::instance().rank();
  const Uint nproc=(Uint)mpi::PE::instance().size();

  assert(gid.size()==rank.size());

  // count how much to send to each process
  // fill send map directly
  // and also fill updatable
  std::vector< std::vector<Uint> > sendmap;
  m_updatable.resize(gid.size());
  sendmap.resize(nproc);
  for(std::vector<Uint>::iterator i=rank.begin(); i < rank.end(); i++){
    if (*i!=irank){
      m_sendCount[*i]++;
      sendmap[*i].push_back(gid[i-rank.begin()]);
    }
    m_updatable[i-rank.begin()]=(*i==irank);
  }
  for(std::vector< std::vector<Uint> >::iterator i=sendmap.begin(); i < sendmap.end(); i++)
    for(std::vector<Uint>::iterator j=i->begin(); j < i->end(); j++)
      m_sendMap.push_back(*j);

  // fill receive count
  boost::mpi::all_to_all(mpi::PE::instance(),m_sendCount,m_receiveCount);

  // fill receive map
  Uint total_m_receiveCount=0;
  for(Uint i=0; i<nproc; i++)
    total_m_receiveCount+=m_receiveCount[i];

  m_receiveMap.resize(total_m_receiveCount);
  std::vector< int > rcvdisp(nproc,0);

  for(Uint i=1; i<nproc; i++)
    rcvdisp[i]=rcvdisp[i-1]+m_receiveCount[i-1];

  for(Uint i=0; i<nproc; i++)
    MPI_Gatherv(&(sendmap[i])[0], sendmap[i].size(), MPI_INT, &m_receiveMap[0], &m_receiveCount[0], &rcvdisp[0], MPI_INT, i, mpi::PE::instance());

  std::cout << "m_updatable: ";
  std::for_each(m_updatable.begin(), m_updatable.end(), std::cout << _1 << ' ');
  std::cout << "\n" << std::flush;

  std::cout << "m_sendCount: ";
  std::for_each(m_sendCount.begin(), m_sendCount.end(), std::cout << _1 << ' ');
  std::cout << "\n" << std::flush;

  std::cout << "m_sendMap: ";
  std::for_each(m_sendMap.begin(), m_sendMap.end(), std::cout << _1 << ' ');
  std::cout << "\n" << std::flush;

  std::cout << "m_receiveCount: ";
  std::for_each(m_receiveCount.begin(), m_receiveCount.end(), std::cout << _1 << ' ');
  std::cout << "\n" << std::flush;

  std::cout << "m_receiveMap: ";
  std::for_each(m_receiveMap.begin(), m_receiveMap.end(), std::cout << _1 << ' ');
  std::cout << "\n" << std::flush;
}

////////////////////////////////////////////////////////////////////////////////

  }  // Common
} // CF
*/
