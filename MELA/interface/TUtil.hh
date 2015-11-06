// March 28 2011
// S. Jindariani (sergo@fnal.gov)
// Y. Gao (ygao@fnal.gov)
// K. Burkett (burkett@fnal.gov)


#ifndef ZZ_COMMON
#define ZZ_COMMON
#include <TLorentzVector.h>
#include <TTree.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH1D.h>
#include <TH2D.h>
#include <string>
#include <vector>
#include <TFile.h>
#include <TF1.h>
#include "TVar.hh"
// Misc.
#include "TModMisc.hh"
// Higgs decay ME
#include "TModHiggsMatEl.hh"
#include "TModGravitonMatEl.hh"
#include "TModZprimeMatEl.hh"
// Higgs + 1/2 jets
#include "TModHiggsJJMatEl.hh"
#include "TModHiggsJMatEl.hh"
// VH
#include "TModVHiggsMatEl.hh"
// ttH
#include "ZZMatrixElement/MELA/interface/TModTTBHMatEl.hh"
// NNPDF Driver for JHUGen
#include "ZZMatrixElement/MELA/interface/TNNPDFDriver.hh"


using namespace std;
TString DbnEventLepSelName(int i);
void My_choose(TVar::Process process, TVar::Production production, TVar::LeptonInterference leptonInterf, int flavor);
void SetEwkCouplingParameters();
void SetAlphaS(double Q_ren, double Q_fac, double multiplier_ren, double multiplier_fac, int mynloop, int mynflav, string mypartons);
void SetMCFMHiggsDecayCouplings(bool useBSM, double Hvvcoupl[SIZE_HVV][2], double Hwwcoupl[SIZE_HVV][2]);
bool My_smalls(double s[][mxpart], int npart);
double SumMatrixElementPDF(
  TVar::Process procees, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, mcfm_event_type* mcfm_event, MelaOutputRecord* RcdME,
  double* flux, double EBEAM, double coupling[SIZE_HVV_FREENORM]
  );
double JHUGenMatEl(
  TVar::Process process, TVar::Production production, mcfm_event_type* mcfm_event, double MReso, double GaReso,
  double Hggcoupl[SIZE_HGG][2], double Hvvcoupl[SIZE_HVV][2], double Zqqcoupl[SIZE_ZQQ][2], double Zvvcoupl[SIZE_ZVV][2],
  double Gqqcoupl[SIZE_GQQ][2], double Gggcoupl[SIZE_GGG][2], double Gvvcoupl[SIZE_GVV][2]
  );
double HJJMatEl(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement, event_scales_type* event_scales, MelaOutputRecord* RcdME,
  const TLorentzVector p[5], double Hggcoupl[SIZE_HGG][2], double Hvvcoupl[SIZE_HVV_VBF][2], double Hwwcoupl[SIZE_HWW_VBF][2],
  TVar::VerbosityLevel verb, double EBEAM
  );
double VHiggsMatEl(
  TVar::Process process, TVar::Production production, event_scales_type* event_scales, MelaOutputRecord* RcdME,
  TLorentzVector p[5], TLorentzVector pHdaughter[4], int Vdecay_id[6],
  double MReso, double GaReso, double Hvvcoupl[SIZE_HVV_VBF][2], TVar::VerbosityLevel verbosity, double EBEAM
  );
double TTHiggsMatEl(
  TVar::Production production, const TLorentzVector p[11],
  double MReso, double GaReso, double MFerm, double GaFerm, double Hvvcoupl[SIZE_TTH][2],
  int topDecay, int topProcess, TVar::VerbosityLevel verbosity
  );
bool CheckPartonMomFraction(const TLorentzVector p0, const TLorentzVector p1, double xx[2], TVar::VerbosityLevel verbosity, double EBEAM);
void ComputePDF(const TLorentzVector p0, const TLorentzVector p1, double fx1[nmsq], double fx2[nmsq], TVar::VerbosityLevel verbosity, double EBEAM);
double SumMEPDF(const TLorentzVector p0, const TLorentzVector p1, double flavor_msq[nmsq][nmsq], MelaOutputRecord* RcdME, TVar::VerbosityLevel verbosity, double EBEAM);
double InterpretScaleScheme(TVar::Production production, TVar::MatrixElement matrixElement, TVar::EventScaleScheme scheme, TLorentzVector p[mxpart]);
void SetDecayModes(int first, int second);
void GetDecayModes(int& first, int& second);

#endif
