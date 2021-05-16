////////////////////////////////////////////////////////////////////
//
//   THaEvData
//   Hall A Event Data from One "Event"
//
//   This is a pure virtual base class.  You should not
//   instantiate this directly (and actually CAN not), but
//   rather use THaCodaDecoder or (less likely) a sim class like
//   THaVDCSimDecoder.
//
//   This class is intended to provide a crate/slot structure
//   for derived classes to use.  All derived class must define and
//   implement LoadEvent(const int*).  See the header.
//
//   original author  Robert Michaels (rom@jlab.org)
//
//   modified for abstraction by Ken Rossato (rossato@jlab.org)
//
//
/////////////////////////////////////////////////////////////////////

#include "THaEvData.h"
#include "Module.h"
#include "THaSlotData.h"
#include "THaCrateMap.h"
#include "THaBenchmark.h"
#include "TError.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <iostream>
#include <iomanip>
#include <ctime>

using namespace std;
using namespace Decoder;

// Instances of this object
TBits THaEvData::fgInstances;

const Double_t THaEvData::kBig = 1e38;

// If false, signal attempted use of unimplemented features
#ifndef NDEBUG
Bool_t THaEvData::fgAllowUnimpl = false;
#else
Bool_t THaEvData::fgAllowUnimpl = true;
#endif

TString THaEvData::fgDefaultCrateMapName = "cratemap";

//_____________________________________________________________________________

THaEvData::THaEvData() :
  fMap{nullptr},
  crateslot{new THaSlotData*[MAXROC*MAXSLOT]},  // FIXME: allocate dynamically
  first_decode{true},
  fTrigSupPS{true},
  fMultiBlockMode{false},
  fBlockIsDone{false},
  fDataVersion{0},
  fEpicsEvtType{Decoder::EPICS_EVTYPE},  // default for Hall A
  buffer{nullptr},
  fDebugFile{nullptr},
  event_type{0},
  event_length{0},
  event_num{0},
  run_num{0},
  evscaler{0},
  bank_tag{0},
  data_type{0},
  block_size{0},
  tbLen{0},
  run_type{0},
  fRunTime(time(nullptr)),    // default fRunTime is NOW
  evt_time{0},
  recent_event{0},
  buffmode{false},
  synchmiss{false},
  synchextra{false},
  fNSlotUsed{0},
  fNSlotClear{0},
  fSlotUsed{new UShort_t[MAXROC*MAXSLOT]},
  fSlotClear{new UShort_t[MAXROC*MAXSLOT]},
  fDoBench{false},
  fBench{nullptr},
  fInstance{fgInstances.FirstNullBit()},
  fNeedInit{true},
  fDebug{0},
  fExtra{nullptr}
{
  fgInstances.SetBitNumber(fInstance);
  fInstance++;
  memset(bankdat,0,MAXBANK*MAXROC*sizeof(BankDat_t));
  memset(crateslot,0,MAXROC*MAXSLOT*sizeof(void*));
}

//_____________________________________________________________________________
THaEvData::~THaEvData() {
  if( fDoBench ) {
    Float_t a,b;
    fBench->Summary(a,b);
  }
  delete fBench;
  // We must delete every array element since not all may be in fSlotUsed.
  for( UInt_t i = 0; i < MAXROC*MAXSLOT; i++ )
    delete crateslot[i];
  delete [] crateslot;
  delete [] fSlotUsed;
  delete [] fSlotClear;
  delete fMap;
  fInstance--;
  fgInstances.ResetBitNumber(fInstance);
}

//_____________________________________________________________________________
const char* THaEvData::DevType( UInt_t crate, UInt_t slot) const {
// Device type in crate, slot
  return ( GoodIndex(crate,slot) ) ?
    crateslot[idx(crate,slot)]->devType() : " ";
}

//_____________________________________________________________________________
Int_t THaEvData::Init()
{
  Int_t ret = init_cmap();
  //  if (fMap) fMap->print();
  if (ret != HED_OK) return ret;
  ret = init_slotdata();
  first_decode = false;
  fNeedInit = false;
  fMsgPrinted.ResetAllBits();
  return ret;
}

//_____________________________________________________________________________
void THaEvData::SetRunTime( ULong64_t tloc )
{
  // Set run time and re-initialize crate map (and possibly other
  // database parameters for the new time.
  if( fRunTime == tloc )
    return;
  fRunTime = tloc;

  init_cmap();
}

//_____________________________________________________________________________
void THaEvData::EnableBenchmarks( Bool_t enable )
{
  // Enable/disable run time reporting
  fDoBench = enable;
  if( fDoBench ) {
    if( !fBench )
      fBench = new THaBenchmark;
  } else {
    delete fBench;
    fBench = nullptr;
  }
}

//_____________________________________________________________________________
void THaEvData::EnableHelicity( Bool_t enable )
{
  // Enable/disable helicity decoding
  SetBit(kHelicityEnabled, enable);
}

//_____________________________________________________________________________
void THaEvData::EnableScalers( Bool_t enable )
{
  // Enable/disable scaler decoding
  SetBit(kScalersEnabled, enable);
}

//_____________________________________________________________________________
void THaEvData::SetVerbose( Int_t level )
{
  // Set verbosity level. Identical to SetDebug(). Kept for compatibility.

  SetDebug(level);
}

//_____________________________________________________________________________
void THaEvData::SetDebug( Int_t level )
{
  // Set debug level

  fDebug = level;
}

//_____________________________________________________________________________
void THaEvData::SetOrigPS(Int_t evtyp)
{
  switch(evtyp) {
  case TS_PRESCALE_EVTYPE: // default after Nov 2003
    fTrigSupPS = true;
    break;
  case PRESCALE_EVTYPE:
    fTrigSupPS = false;
    break;
  default:
    cerr << "SetOrigPS::Warn: PS factors";
    cerr << " originate only from evtype ";
    cerr << PRESCALE_EVTYPE << "  or ";
    cerr << TS_PRESCALE_EVTYPE << endl;
    break;
  }
}

//_____________________________________________________________________________
TString THaEvData::GetOrigPS() const
{
  TString answer = "PS from ";
  if (fTrigSupPS) {
    answer += " Trig Sup evtype ";
    answer.Append(Form("%d",TS_PRESCALE_EVTYPE));
  } else {
    answer += " PS evtype ";
    answer.Append(Form("%d",PRESCALE_EVTYPE));
  }
  return answer;
}

//_____________________________________________________________________________
void THaEvData::hexdump(const char* cbuff, size_t nlen)
{
  // Hexdump buffer 'cbuff' of length 'nlen'
  const int NW = 16; const char* p = cbuff;
  while( p<cbuff+nlen ) {
    cout << dec << setw(4) << setfill('0') << (size_t)(p-cbuff) << " ";
    Long_t nelem = TMath::Min((Long_t)NW,(Long_t)(cbuff+nlen-p));
    for(int i=0; i<NW; i++) {
      UInt_t c = (i<nelem) ? *(const unsigned char*)(p+i) : 0;
      cout << " " << hex << setfill('0') << setw(2) << c << dec;
    } cout << setfill(' ') << "  ";
    for(int i=0; i<NW; i++) {
      char c = (i<nelem) ? *(p+i) : (char)0;
      if(isgraph(c)||c==' ') cout << c; else cout << ".";
    } cout << endl;
    p += NW;
  }
}

//_____________________________________________________________________________
void THaEvData::SetDefaultCrateMapName( const char* name )
{
  // Static function to set fgDefaultCrateMapName. Call this function to set a
  // global default name for all decoder instances before initialization. This
  // is usually what you want to do for a given replay.

  if( name && *name ) {
    fgDefaultCrateMapName = name;
  }
  else {
    ::Error( "THaEvData::SetDefaultCrateMapName", "Default crate map name "
	     "must not be empty" );
  }
}

//_____________________________________________________________________________
void THaEvData::SetCrateMapName( const char* name )
{
  // Set fCrateMapName for this decoder instance only

  if( name && *name ) {
    if( fCrateMapName != name ) {
      fCrateMapName = name;
      fNeedInit = true;
    }
  } else if( fCrateMapName != fgDefaultCrateMapName ) {
    fCrateMapName = fgDefaultCrateMapName;
    fNeedInit = true;
  }
}

//_____________________________________________________________________________
// Set up and initialize the crate map
int THaEvData::init_cmap()  {
  if( fCrateMapName.IsNull() )
    fCrateMapName = fgDefaultCrateMapName;
  if( !fMap || fNeedInit || fCrateMapName != fMap->GetName() ) {
    delete fMap;
    fMap = new THaCrateMap( fCrateMapName );
  }
  if( fDebug>0 )
    cout << "Initializing crate map " << endl;
  FILE* fi; TString fname; Int_t ret;
  if( init_cmap_openfile(fi,fname) != 0 ) {
    // A derived class implements a special method to open the crate map
    // database file. Call THaCrateMap's file-based init method.
    ret = fMap->init(fi,fname);
  } else {
    // Use the default behavior of THaCrateMap for initializing the map
    // (currently that means opening a database file named fCrateMapName)
    ret = fMap->init(GetRunTime());
  }
  if( ret == THaCrateMap::CM_ERR )
    return HED_FATAL; // Can't continue w/o cratemap
  fNeedInit = false;
  return HED_OK;
}

//_____________________________________________________________________________
void THaEvData::makeidx( UInt_t crate, UInt_t slot )
{
  // Activate crate/slot
  UInt_t idx = slot+MAXSLOT*crate;
  delete crateslot[idx];  // just in case
  crateslot[idx] = new THaSlotData(crate,slot);
  if (fDebugFile) crateslot[idx]->SetDebugFile(fDebugFile);
  if( !fMap ) return;
  if( fMap->crateUsed(crate) && fMap->slotUsed(crate,slot)) {
    crateslot[idx]
      ->define( crate, slot, fMap->getNchan(crate,slot),
		fMap->getNdata(crate,slot) );
    fSlotUsed[fNSlotUsed++] = idx;
    if( fMap->slotClear(crate,slot))
      fSlotClear[fNSlotClear++] = idx;
    crateslot[idx]->loadModule(fMap);
  }
}

//_____________________________________________________________________________
void THaEvData::PrintOut() const {
  //TODO
  cout << "THaEvData::PrintOut() called" << endl;
}

//_____________________________________________________________________________
void THaEvData::PrintSlotData( UInt_t crate, UInt_t slot) const {
  // Print the contents of (crate, slot).
  if( GoodIndex(crate,slot)) {
    crateslot[idx(crate,slot)]->print();
  } else {
      cout << "THaEvData: Warning: Crate, slot combination";
      cout << "\nexceeds limits.  Cannot print"<<endl;
  }
}

//_____________________________________________________________________________
// To initialize the THaSlotData member on first call to decoder
int THaEvData::init_slotdata()
{
  // Update lists of used/clearable slots in case crate map changed
  if(!fMap) return HED_ERR;
  for( UInt_t i=0; i<fNSlotUsed; i++ ) {
    THaSlotData* module = crateslot[fSlotUsed[i]];
    UInt_t crate = module->getCrate();
    UInt_t slot  = module->getSlot();
    if( !fMap->crateUsed(crate) || !fMap->slotUsed(crate,slot) ||
	!fMap->slotClear(crate,slot)) {
      for( UInt_t k = 0; k < fNSlotClear; k++ ) {
	if( module == crateslot[fSlotClear[k]] ) {
	  for( UInt_t j=k+1; j<fNSlotClear; j++ )
	    fSlotClear[j-1] = fSlotClear[j];
	  fNSlotClear--;
	  break;
	}
      }
    }
    if( !fMap->crateUsed(crate) || !fMap->slotUsed(crate, slot) ) {
      for( UInt_t j = i+1; j < fNSlotUsed; j++ )
        fSlotUsed[j-1] = fSlotUsed[j];
      fNSlotUsed--;
    }
  }
  return HED_OK;
}

//_____________________________________________________________________________
void THaEvData::FindUsedSlots() {
  // Disable slots for which no module is defined.
  // This speeds up the decoder.
  for( UInt_t roc = 0; roc < MAXROC; roc++ ) {
    for( UInt_t slot = 0; slot < MAXSLOT; slot++ ) {
      if ( !fMap->slotUsed(roc,slot) ) continue;
      if ( !crateslot[idx(roc,slot)]->GetModule() ) {
	cout << "WARNING:  No module defined for crate "<<roc<<"   slot "<<slot<<endl;
	cout << "Check db_cratemap.dat for module that is undefined"<<endl;
	cout << "This crate, slot will be ignored"<<endl;
	fMap->setUnused(roc,slot);
      }
    }
  }
}

//_____________________________________________________________________________
Module* THaEvData::GetModule( UInt_t roc, UInt_t slot) const
{
  THaSlotData *sldat = crateslot[idx(roc,slot)];
  if (sldat) return sldat->GetModule();
  return nullptr;
}

//_____________________________________________________________________________
Int_t THaEvData::SetDataVersion( Int_t version )
{
  return (fDataVersion = version);
}

ClassImp(THaEvData)
ClassImp(THaBenchmark)
