#ifndef ROOT_THaPrimaryKine
#define ROOT_THaPrimaryKine

//////////////////////////////////////////////////////////////////////////
//
// THaPrimaryKine
//
//////////////////////////////////////////////////////////////////////////

#include "THaPhysicsModule.h"
#include "TLorentzVector.h"
#include "TString.h"

class THaTrackingModule;
typedef TLorentzVector FourVect;

class THaPrimaryKine : public THaPhysicsModule {
  
public:
  THaPrimaryKine( const char* name, const char* description,
		  const char* spectro = "",
		  Double_t particle_mass = 0.0, /* GeV */
		  Double_t target_mass = 0.0 /* GeV */ );
  virtual ~THaPrimaryKine();
  
  virtual void      Clear( Option_t* opt="" );

  Double_t          GetQ2()         const { return fQ2; }
  Double_t          GetOmega()      const { return fOmega; }
  Double_t          GetNu()         const { return fOmega; }
  Double_t          GetW2()         const { return fW2; }
  Double_t          GetXbj()        const { return fXbj; }
  Double_t          GetScatAngle()  const { return fScatAngle; }
  Double_t          GetEpsilon()    const { return fEpsilon; }
  Double_t          GetQ3mag()      const { return fQ3mag; }
  Double_t          GetThetaQ()     const { return fThetaQ; }
  Double_t          GetPhiQ()       const { return fPhiQ; }
  Double_t          GetMass()       const { return fM; }
  Double_t          GetTargetMass() const { return fMA; }

  const FourVect*   GetP0()         const { return &fP0; }
  const FourVect*   GetP1()         const { return &fP1; }
  const FourVect*   GetA()          const { return &fA; }
  const FourVect*   GetA1()         const { return &fA1; }
  const FourVect*   GetQ()          const { return &fQ; }

  virtual EStatus   Init( const TDatime& run_time );
  virtual Int_t     Process( const THaEvData& );
          void      SetMass( Double_t m );
          void      SetTargetMass( Double_t m );
          void      SetSpectrometer( const char* name );

protected:

  Double_t          fQ2;           // 4-momentum transfer squared (GeV^2)
  Double_t          fOmega;        // Energy transfer (GeV)
  Double_t          fW2;           // Invariant mass of recoil system (GeV^2)
  Double_t          fXbj;          // x Bjorken
  Double_t          fScatAngle;    // Scattering angle (rad)
  Double_t          fEpsilon;      // Virtual photon polarization factor
  Double_t          fQ3mag;        // Magnitude of 3-momentum transfer
  Double_t          fThetaQ;       // Theta of 3-momentum vector (rad)
  Double_t          fPhiQ;         // Phi of 3-momentum transfer (rad)
  FourVect          fP0;           // Beam 4-momentum
  FourVect          fP1;           // Scattered electron 4-momentum
  FourVect          fA;            // Target 4-momentum
  FourVect          fA1;           // Recoil system 4-momentum
  FourVect          fQ;            // Momentum transfer 4-vector

  Double_t          fM;            // Mass of particle (GeV/c^2)
  Double_t          fMA;           // Effective mass of target (GeV/c^2)

  virtual Int_t DefineVariables( EMode mode = kDefine );
  virtual Int_t ReadRunDatabase( const TDatime& date );

  void PrintInitError( const char* here );

  TString                 fSpectroName;  // Name of spectrometer to consider
  THaTrackingModule*      fSpectro;      // Pointer to spectrometer object

  ClassDef(THaPrimaryKine,0)   //Single arm kinematics module
};

#endif