using namespace std;
#include <iostream>
// #include <iomanip>
#include <sstream>
#include <string>
#include <typeinfo>
#include <assert.h>
#include <cstdlib>
#include <cstdio>
#include <chrono>

#include "snapshot.h"
#include "mymath.h"
#include <mpi.h>

ostream& operator << (ostream& o, Particle_t &p)
{
   o << "[" << p.Id << ", " <<p.Mass<<", " << p.ComovingPosition << ", " << p.PhysicalVelocity << "]";
   return o;
};
/*
void ParticleSnapshot_t::FillParticleHash()
{
  cout<<"Filling Hash Table...\n";
  auto begin = chrono::high_resolution_clock::now();
  ParticleHash.rehash(NumberOfParticles);
  ParticleHash.reserve(NumberOfParticles);
  for(HBTInt i=0;i<NumberOfParticles;i++)
	ParticleHash[ParticleId[i]]=i;
  auto end = chrono::high_resolution_clock::now();
  auto elapsed = chrono::duration_cast<std::chrono::milliseconds>(end - begin);
  cout << "inserts: " << elapsed.count() << endl;
//   cout<<ParticleHash.bucket_count()<<" buckets used; load factor "<<ParticleHash.load_factor()<<endl;
}
void ParticleSnapshot_t::ClearParticleHash()
{
  ParticleHash.clear();
}
*/
class ParticleKeyList_t: public KeyList_t <HBTInt, HBTInt>
{
  typedef HBTInt Index_t;
  typedef HBTInt Key_t;
  const ParticleSnapshot_t &Snap;
public:
  ParticleKeyList_t(ParticleSnapshot_t &snap): Snap(snap) {};
  Index_t size() const
  {
	return Snap.size();
  }
  Key_t GetKey(Index_t i) const
  {
	return Snap.GetId(i);
  }
};

void ParticleSnapshot_t::FillParticleHash()
{
  if(HBTConfig.ParticleIdNeedHash)
	ParticleHash=&MappedHash;
  else
	ParticleHash=&FlatHash;

  ParticleKeyList_t Ids(*this); 
  ParticleHash->Fill(Ids);
}
void ParticleSnapshot_t::ClearParticleHash()
{
  ParticleHash->Clear();
}

void ParticleSnapshot_t::AveragePosition(HBTxyz& CoM, const HBTInt Particles[], HBTInt NumPart) const
/*mass weighted average position*/
{
	HBTInt i,j;
	double sx[3],origin[3],msum;
	
	if(0==NumPart) return;
	if(1==NumPart) 
	{
	  copyHBTxyz(CoM, GetComovingPosition(Particles[0]));
	  return;
	}
	
	sx[0]=sx[1]=sx[2]=0.;
	msum=0.;
	if(HBTConfig.PeriodicBoundaryOn)
	  for(j=0;j<3;j++)
		origin[j]=GetComovingPosition(Particles[0])[j];
	
	for(i=0;i<NumPart;i++)
	{
	  HBTReal m=GetMass(Particles[i]);
	  msum+=m;
	  for(j=0;j<3;j++)
	  if(HBTConfig.PeriodicBoundaryOn)
		  sx[j]+=NEAREST(GetComovingPosition(Particles[i])[j]-origin[j])*m;
	  else
		  sx[j]+=GetComovingPosition(Particles[i])[j]*m;
	}
	
	for(j=0;j<3;j++)
	{
		sx[j]/=msum;
		if(HBTConfig.PeriodicBoundaryOn) sx[j]+=origin[j];
		CoM[j]=sx[j];
	}
}
void ParticleSnapshot_t::AverageVelocity(HBTxyz& CoV, const HBTInt Particles[], HBTInt NumPart) const
/*mass weighted average velocity*/
{
	HBTInt i,j;
	double sv[3],msum;
	
	if(0==NumPart) return;
	if(1==NumPart) 
	{
	  copyHBTxyz(CoV, GetPhysicalVelocity(Particles[0]));
	  return;
	}
	
	sv[0]=sv[1]=sv[2]=0.;
	msum=0.;
	
	for(i=0;i<NumPart;i++)
	{
	  HBTReal m=GetMass(Particles[i]);
	  msum+=m;
	  for(j=0;j<3;j++)
		sv[j]+=GetPhysicalVelocity(Particles[i])[j]*m;
	}
	
	for(j=0;j<3;j++)
	  CoV[j]=sv[j]/msum;
}

void create_MPI_Particle_type(MPI_Datatype &MPI_HBTParticle_t)
{
/*to create the struct data type for communication*/	
Particle_t p;
#define NumAttr 4
MPI_Datatype oldtypes[NumAttr];
MPI_Aint   offsets[NumAttr], origin,extent;
int blockcounts[NumAttr];

MPI_Get_address(&p,&origin);
MPI_Get_address(&p.Id,offsets);
MPI_Get_address(p.ComovingPosition.data(),offsets+1);//caution: this might be implementation dependent??
MPI_Get_address(p.PhysicalVelocity.data(),offsets+2);
MPI_Get_address(&p.Mass,offsets+3);
MPI_Get_address((&p)+1,&extent);//to get the extent of s

for(int i=0;i<NumAttr;i++)
  offsets[i]-=origin;

oldtypes[0] = MPI_HBT_INT;
blockcounts[0] = 1;

oldtypes[1] = MPI_HBT_REAL;
blockcounts[1] = 3;

oldtypes[2] = MPI_HBT_REAL;
blockcounts[2] = 3;

oldtypes[3] = MPI_HBT_REAL;
blockcounts[3] = 1;

extent-=origin;

assert(offsets[2]-offsets[1]==sizeof(HBTReal)*3);//to make sure HBTxyz is stored locally.
/*
for(int i=0;i<NumAttr;i++)
  cout<<offsets[i]<<',';
cout<<endl<<extent<<endl;
*/
MPI_Type_create_struct(NumAttr,blockcounts,offsets,oldtypes, &MPI_HBTParticle_t);//some padding is added automatically by MPI as well
MPI_Type_create_resized(MPI_HBTParticle_t,(MPI_Aint)0, extent, &MPI_HBTParticle_t);
MPI_Type_commit(&MPI_HBTParticle_t);
//~ MPI_Type_get_extent(*pMPIshearprof,&origin,&extent);
//~ printf("%d\n",extent);
#undef NumAttr
}

void AveragePosition(HBTxyz& CoM, const Particle_t Particles[], HBTInt NumPart)
/*mass weighted average position*/
{
	HBTInt i,j;
	double sx[3],origin[3],msum;
	
	if(0==NumPart) return;
	if(1==NumPart) 
	{
	  copyHBTxyz(CoM, Particles[0].ComovingPosition);
	  return;
	}
	
	sx[0]=sx[1]=sx[2]=0.;
	msum=0.;
	if(HBTConfig.PeriodicBoundaryOn)
	  for(j=0;j<3;j++)
		origin[j]=Particles[0].ComovingPosition[j];
	
	for(i=0;i<NumPart;i++)
	{
	  HBTReal m=Particles[i].Mass;
	  msum+=m;
	  for(j=0;j<3;j++)
	  if(HBTConfig.PeriodicBoundaryOn)
		  sx[j]+=NEAREST(Particles[i].ComovingPosition[j]-origin[j])*m;
	  else
		  sx[j]+=Particles[i].ComovingPosition[j]*m;
	}
	
	for(j=0;j<3;j++)
	{
		sx[j]/=msum;
		if(HBTConfig.PeriodicBoundaryOn) sx[j]+=origin[j];
		CoM[j]=sx[j];
	}
}
void AverageVelocity(HBTxyz& CoV, const Particle_t Particles[], HBTInt NumPart)
/*mass weighted average velocity*/
{
	HBTInt i,j;
	double sv[3],msum;
	
	if(0==NumPart) return;
	if(1==NumPart) 
	{
	  copyHBTxyz(CoV, Particles[0].PhysicalVelocity);
	  return;
	}
	
	sv[0]=sv[1]=sv[2]=0.;
	msum=0.;
	
	for(i=0;i<NumPart;i++)
	{
	  HBTReal m=Particles[i].Mass;
	  msum+=m;
	  for(j=0;j<3;j++)
		sv[j]+=Particles[i].PhysicalVelocity[j]*m;
	}
	
	for(j=0;j<3;j++)
	  CoV[j]=sv[j]/msum;
}