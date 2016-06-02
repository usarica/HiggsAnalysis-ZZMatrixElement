#include <ZZMatrixElement/MELA/interface/TUtil.hh>
#include <iostream>
#include <cstdio>
#include <cmath>
#include <utility>
#include <algorithm>
#include "TMath.h"
#include "TLorentzRotation.h"


using namespace std;


namespace TUtil{
  bool forbidMassiveLeptons = true;
  bool forbidMassiveJets = true;
  TVar::FermionMassRemoval LeptonMassScheme = TVar::ConserveDifermionMass;
  TVar::FermionMassRemoval JetMassScheme = TVar::MomentumToEnergy;
}

/***************************************************/
/***** Scripts for decay and production angles *****/
/***************************************************/

void TUtil::applyLeptonMassCorrection(bool flag){ TUtil::forbidMassiveLeptons = flag; }
void TUtil::applyJetMassCorrection(bool flag){ TUtil::forbidMassiveJets = flag; }
void TUtil::setLeptonMassScheme(TVar::FermionMassRemoval scheme){ LeptonMassScheme=scheme; }
void TUtil::setJetMassScheme(TVar::FermionMassRemoval scheme){ JetMassScheme=scheme; }
void TUtil::constrainedRemovePairMass(TLorentzVector& p1, TLorentzVector& p2, double m1, double m2){
  TLorentzVector nullFourVector(0, 0, 0, 0);
  TLorentzVector p1hat, p2hat;
  if (p1==nullFourVector || p2==nullFourVector) return;

  /***** shiftMass in C++ *****/
  TLorentzVector p12=p1+p2;
  TLorentzVector diffp2p1=p2-p1;
  double p1sq = p1.M2();
  double p2sq = p2.M2();
  double p1p2 = p1.Dot(p2);
  double m1sq = m1*fabs(m1);
  double m2sq = m2*fabs(m2);
  double p12sq = p12.M2();

  TLorentzVector avec=(p1sq*p2 - p2sq*p1 + p1p2*diffp2p1);
  double a = avec.M2();
  double b = (p12sq + m2sq - m1sq) * (pow(p1p2, 2) - p1sq*p2sq);
  double c = pow((p12sq + m2sq - m1sq), 2)*p1sq/4. - pow((p1sq + p1p2), 2)*m2sq;
  double eta =  (-b - sqrt(fabs(b*b -4.*a*c)))/(2.*a);
  double xi = (p12sq + m2sq - m1sq - 2.*eta*(p2sq + p1p2))/(2.*(p1sq + p1p2));

  p1hat = (1.-xi)*p1 + (1.-eta)*p2;
  p2hat = xi*p1 + eta*p2;
  p1=p1hat;
  p2=p2hat;
}
void TUtil::scaleMomentumToEnergy(TLorentzVector massiveJet, TLorentzVector& masslessJet, double mass){
  double energy, p3, newp3, ratio;
  energy = massiveJet.T();
  p3 = massiveJet.P();
  newp3 = sqrt(max(pow(energy, 2)-mass*fabs(mass), 0.));
  ratio = (p3>0. ? (newp3/p3) : 1.);
  masslessJet.SetXYZT(massiveJet.X()*ratio, massiveJet.Y()*ratio, massiveJet.Z()*ratio, energy);
}
pair<TLorentzVector, TLorentzVector> TUtil::removeMassFromPair(
  TLorentzVector jet1, int jet1Id,
  TLorentzVector jet2, int jet2Id,
  double m1, double m2
  ){
  TLorentzVector nullFourVector(0, 0, 0, 0);
  TLorentzVector jet1massless, jet2massless;

  if (TUtil::forbidMassiveJets && (PDGHelpers::isAJet(jet1Id) || PDGHelpers::isAJet(jet2Id))){
    if (JetMassScheme==TVar::NoRemoval){
      jet1massless=jet1;
      jet2massless=jet2;
    }
    else if (jet1==nullFourVector || jet2==nullFourVector || JetMassScheme==TVar::MomentumToEnergy){
      TUtil::scaleMomentumToEnergy(jet1, jet1massless, m1);
      TUtil::scaleMomentumToEnergy(jet2, jet2massless, m2);
    }
    else if (JetMassScheme==TVar::ConserveDifermionMass){
      jet1massless=jet1;
      jet2massless=jet2;
      TUtil::constrainedRemovePairMass(jet1massless, jet2massless, m1, m2);
    }
    else{
      jet1massless=jet1;
      jet2massless=jet2;
    }
  }
  else if (TUtil::forbidMassiveLeptons && (PDGHelpers::isALepton(jet1Id) || PDGHelpers::isANeutrino(jet1Id) || PDGHelpers::isALepton(jet2Id) || PDGHelpers::isANeutrino(jet2Id))){
    if (forbidMassiveLeptons==TVar::NoRemoval){
      jet1massless=jet1;
      jet2massless=jet2;
    }
    else if (jet1==nullFourVector || jet2==nullFourVector || LeptonMassScheme==TVar::MomentumToEnergy){
      TUtil::scaleMomentumToEnergy(jet1, jet1massless, m1);
      TUtil::scaleMomentumToEnergy(jet2, jet2massless, m2);
    }
    else if (LeptonMassScheme==TVar::ConserveDifermionMass){
      jet1massless=jet1;
      jet2massless=jet2;
      TUtil::constrainedRemovePairMass(jet1massless, jet2massless, m1, m2);
    }
    else{
      jet1massless=jet1;
      jet2massless=jet2;
    }
  }
  else{
    jet1massless=jet1;
    jet2massless=jet2;
  }

  pair<TLorentzVector, TLorentzVector> result(jet1massless, jet2massless);
  return result;
}
void TUtil::adjustTopDaughters(SimpleParticleCollection_t& daughters){ // Daughters are arranged as b, Wf, Wfb
  if (daughters.size()!=3) return; // Cannot work if the number of daughters is not exactly 3.
  TUtil::removeMassFromPair(
    daughters.at(1).second, daughters.at(1).first,
    daughters.at(2).second, daughters.at(2).first
    );
  TLorentzVector pW = daughters.at(1).second+daughters.at(2).second;
  TVector3 pW_boost_old = -pW.BoostVector();
  TUtil::removeMassFromPair(
    daughters.at(0).second, daughters.at(0).first,
    pW, -daughters.at(0).first, // Trick the function
    0., pW.M() // Conserve W mass, ensures Wf+Wfb=W after re-boosting Wf and Wfb to te new W frame.
    );
  TVector3 pW_boost_new = pW.BoostVector();
  for (unsigned int idau=1; idau<daughters.size(); idau++){
    daughters.at(idau).second.Boost(pW_boost_old);
    daughters.at(idau).second.Boost(pW_boost_new);
  }
}
// Compute a fake jet
void TUtil::computeFakeJet(TLorentzVector realJet, TLorentzVector others, TLorentzVector& fakeJet){
  TLorentzVector masslessRealJet(0, 0, 0, 0);
  if (TUtil::forbidMassiveJets) TUtil::scaleMomentumToEnergy(realJet, masslessRealJet);
  else masslessRealJet = realJet;
  fakeJet = others + masslessRealJet;
  fakeJet.SetVect(-fakeJet.Vect());
  fakeJet.SetE(fakeJet.P());
}

/***** Decay *****/
void TUtil::computeAngles(
  TLorentzVector p4M11, int Z1_lept1Id,
  TLorentzVector p4M12, int Z1_lept2Id,
  TLorentzVector p4M21, int Z2_lept1Id,
  TLorentzVector p4M22, int Z2_lept2Id,
  float& costhetastar,
  float& costheta1,
  float& costheta2,
  float& Phi,
  float& Phi1
  ){
  TLorentzVector nullFourVector(0, 0, 0, 0);
  if (p4M12==nullFourVector || p4M22==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f13Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M21, Z2_lept1Id);
    p4M11 = f13Pair.first;
    p4M21 = f13Pair.second;
  }
  else if (p4M11==nullFourVector || p4M21==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f24Pair = TUtil::removeMassFromPair(p4M12, Z1_lept2Id, p4M22, Z2_lept2Id);
    p4M12 = f24Pair.first;
    p4M22 = f24Pair.second;
  }
  else{
    pair<TLorentzVector, TLorentzVector> f12Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M12, Z1_lept2Id);
    pair<TLorentzVector, TLorentzVector> f34Pair = TUtil::removeMassFromPair(p4M21, Z2_lept1Id, p4M22, Z2_lept2Id);
    p4M11 = f12Pair.first;
    p4M12 = f12Pair.second;
    p4M21 = f34Pair.first;
    p4M22 = f34Pair.second;
  }

  //build Z 4-vectors
  TLorentzVector p4Z1 = p4M11 + p4M12;
  TLorentzVector p4Z2 = p4M21 + p4M22;

  // Sort Z1 leptons so that:
  if (
    Z1_lept2Id!=9000
    &&
    (
    (Z1_lept1Id*Z1_lept2Id<0 && Z1_lept1Id<0) // for OS pairs: lep1 must be the negative one
    ||
    ((Z1_lept1Id*Z1_lept2Id>0 || (Z1_lept1Id==0 && Z1_lept2Id==0)) && p4M11.Phi()<=p4M12.Phi()) //for SS pairs: use random deterministic convention
    )
    ) swap(p4M11, p4M12);
  // Same for Z2 leptons
  if (
    Z2_lept2Id!=9000
    &&
    (
    (Z2_lept1Id*Z2_lept2Id<0 && Z2_lept1Id<0)
    ||
    ((Z2_lept1Id*Z2_lept2Id>0 || (Z2_lept1Id==0 && Z2_lept2Id==0)) && p4M21.Phi()<=p4M22.Phi())
    )
    ) swap(p4M21, p4M22);

  // BEGIN THE CALCULATION

  // build H 4-vectors
  TLorentzVector p4H = p4Z1 + p4Z2;

  // -----------------------------------

  //// costhetastar
  TVector3 boostX = -(p4H.BoostVector());
  TLorentzVector thep4Z1inXFrame(p4Z1);
  TLorentzVector thep4Z2inXFrame(p4Z2);
  thep4Z1inXFrame.Boost(boostX);
  thep4Z2inXFrame.Boost(boostX);
  TVector3 theZ1X_p3 = TVector3(thep4Z1inXFrame.X(), thep4Z1inXFrame.Y(), thep4Z1inXFrame.Z());
  TVector3 theZ2X_p3 = TVector3(thep4Z2inXFrame.X(), thep4Z2inXFrame.Y(), thep4Z2inXFrame.Z());
  costhetastar = theZ1X_p3.CosTheta();

  TVector3 boostV1(0, 0, 0);
  TVector3 boostV2(0, 0, 0);
  //// --------------------------- costheta1
  if (!(fabs(Z1_lept1Id)==21 || fabs(Z1_lept1Id)==22 || fabs(Z1_lept2Id)==21 || fabs(Z1_lept2Id)==22)){
    boostV1 = -(p4Z1.BoostVector());
    if (boostV1.Mag()>=1.) {
      cout << "Warning: Mela::computeAngles: Z1 boost with beta=1, scaling down" << endl;
      boostV1*=0.9999/boostV1.Mag();
    }
    TLorentzVector p4M11_BV1(p4M11);
    TLorentzVector p4M12_BV1(p4M12);
    TLorentzVector p4M21_BV1(p4M21);
    TLorentzVector p4M22_BV1(p4M22);
    p4M11_BV1.Boost(boostV1);
    p4M12_BV1.Boost(boostV1);
    p4M21_BV1.Boost(boostV1);
    p4M22_BV1.Boost(boostV1);

    TLorentzVector p4V2_BV1 = p4M21_BV1 + p4M22_BV1;
    //// costheta1
    costheta1 = -p4V2_BV1.Vect().Unit().Dot(p4M11_BV1.Vect().Unit());
  }
  else costheta1 = 0;

  //// --------------------------- costheta2
  if (!(fabs(Z2_lept1Id)==21 || fabs(Z2_lept1Id)==22 || fabs(Z2_lept2Id)==21 || fabs(Z2_lept2Id)==22)){
    boostV2 = -(p4Z2.BoostVector());
    if (boostV2.Mag()>=1.) {
      cout << "Warning: Mela::computeAngles: Z2 boost with beta=1, scaling down" << endl;
      boostV2*=0.9999/boostV2.Mag();
    }
    TLorentzVector p4M11_BV2(p4M11);
    TLorentzVector p4M12_BV2(p4M12);
    TLorentzVector p4M21_BV2(p4M21);
    TLorentzVector p4M22_BV2(p4M22);
    p4M11_BV2.Boost(boostV2);
    p4M12_BV2.Boost(boostV2);
    p4M21_BV2.Boost(boostV2);
    p4M22_BV2.Boost(boostV2);

    TLorentzVector p4V1_BV2 = p4M11_BV2 + p4M12_BV2;
    //// costheta2
    costheta2 = -p4V1_BV2.Vect().Unit().Dot(p4M21_BV2.Vect().Unit());
  }
  else costheta2 = 0;

  //// --------------------------- Phi and Phi1 (old phistar1 - azimuthal production angle)
  TLorentzVector p4M11_BX(p4M11);
  TLorentzVector p4M12_BX(p4M12);
  TLorentzVector p4M21_BX(p4M21);
  TLorentzVector p4M22_BX(p4M22);

  p4M11_BX.Boost(boostX);
  p4M12_BX.Boost(boostX);
  p4M21_BX.Boost(boostX);
  p4M22_BX.Boost(boostX);
  TLorentzVector p4V1_BX = p4M11_BX + p4M12_BX;

  TVector3 beamAxis(0, 0, 1);
  TVector3 p3V1_BX = p4V1_BX.Vect().Unit();
  TVector3 normal1_BX = (p4M11_BX.Vect().Cross(p4M12_BX.Vect())).Unit();
  TVector3 normal2_BX = (p4M21_BX.Vect().Cross(p4M22_BX.Vect())).Unit();
  TVector3 normalSC_BX = (beamAxis.Cross(p3V1_BX)).Unit();


  //// Phi
  float tmpSgnPhi = p3V1_BX.Dot(normal1_BX.Cross(normal2_BX));
  float sgnPhi = 0;
  if (fabs(tmpSgnPhi)>0.) sgnPhi = tmpSgnPhi/fabs(tmpSgnPhi);
  float dot_BX12 = normal1_BX.Dot(normal2_BX);
  if (fabs(dot_BX12)>=1.) dot_BX12 *= 1./fabs(dot_BX12);
  Phi = sgnPhi * acos(-1.*dot_BX12);


  //// Phi1
  float tmpSgnPhi1 = p3V1_BX.Dot(normal1_BX.Cross(normalSC_BX));
  float sgnPhi1 = 0;
  if (fabs(tmpSgnPhi1)>0.) sgnPhi1 = tmpSgnPhi1/fabs(tmpSgnPhi1);
  float dot_BX1SC = normal1_BX.Dot(normalSC_BX);
  if (fabs(dot_BX1SC)>=1.) dot_BX1SC *= 1./fabs(dot_BX1SC);
  Phi1 = sgnPhi1 * acos(dot_BX1SC);

  if (isnan(costhetastar) || isnan(costheta1) || isnan(costheta2) || isnan(Phi) || isnan(Phi1)){
    cout << "WARNING: NaN in computeAngles: "
      << costhetastar << " "
      << costheta1  << " "
      << costheta2  << " "
      << Phi  << " "
      << Phi1  << " " << endl;
    cout << "   boostV1: " <<boostV1.Pt() << " " << boostV1.Eta() << " " << boostV1.Phi() << " " << boostV1.Mag() << endl;
    cout << "   boostV2: " <<boostV2.Pt() << " " << boostV2.Eta() << " " << boostV2.Phi() << " " << boostV2.Mag() << endl;
  }
}
void TUtil::computeAnglesCS(
  TLorentzVector p4M11, int Z1_lept1Id,
  TLorentzVector p4M12, int Z1_lept2Id,
  TLorentzVector p4M21, int Z2_lept1Id,
  TLorentzVector p4M22, int Z2_lept2Id,
  float pbeam,
  float& costhetastar,
  float& costheta1,
  float& costheta2,
  float& Phi,
  float& Phi1
  ){
  TLorentzVector nullFourVector(0, 0, 0, 0);
  if (p4M12==nullFourVector || p4M22==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f13Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M21, Z2_lept1Id);
    p4M11 = f13Pair.first;
    p4M21 = f13Pair.second;
  }
  else if (p4M11==nullFourVector || p4M21==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f24Pair = TUtil::removeMassFromPair(p4M12, Z1_lept2Id, p4M22, Z2_lept2Id);
    p4M12 = f24Pair.first;
    p4M22 = f24Pair.second;
  }
  else{
    pair<TLorentzVector, TLorentzVector> f12Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M12, Z1_lept2Id);
    pair<TLorentzVector, TLorentzVector> f34Pair = TUtil::removeMassFromPair(p4M21, Z2_lept1Id, p4M22, Z2_lept2Id);
    p4M11 = f12Pair.first;
    p4M12 = f12Pair.second;
    p4M21 = f34Pair.first;
    p4M22 = f34Pair.second;
  }

  TVector3 LabXaxis(1.0, 0.0, 0.0);
  TVector3 LabYaxis(0.0, 1.0, 0.0);
  TVector3 LabZaxis(0.0, 0.0, 1.0);

  float Mprot = 0.938;
  float Ebeam = sqrt(pbeam*pbeam + Mprot*Mprot);

  TLorentzVector targ(0., 0., -pbeam, Ebeam);
  TLorentzVector beam(0., 0., pbeam, Ebeam);

  //build Z 4-vectors
  TLorentzVector p4Z1 = p4M11 + p4M12;
  TLorentzVector p4Z2 = p4M21 + p4M22;

  // Sort Z1 leptons so that:
  if (
    Z1_lept2Id!=9000
    &&
    (
    (Z1_lept1Id*Z1_lept2Id<0 && Z1_lept1Id<0) // for OS pairs: lep1 must be the negative one
    ||
    ((Z1_lept1Id*Z1_lept2Id>0 || (Z1_lept1Id==0 && Z1_lept2Id==0)) && p4M11.Phi()<=p4M12.Phi()) //for SS pairs: use random deterministic convention
    )
    ) swap(p4M11, p4M12);
  // Same for Z2 leptons
  if (
    Z2_lept2Id!=9000
    &&
    (
    (Z2_lept1Id*Z2_lept2Id<0 && Z2_lept1Id<0)
    ||
    ((Z2_lept1Id*Z2_lept2Id>0 || (Z2_lept1Id==0 && Z2_lept2Id==0)) && p4M21.Phi()<=p4M22.Phi())
    )
    ) swap(p4M21, p4M22);


  //build H 4-vectors
  TLorentzVector p4H = p4Z1 + p4Z2;
  TVector3 boostX = -(p4H.BoostVector());

  /////////////////////////////
  // Collin-Sopper calculation:
  // in the CS frame, the z-axis is along the bisectrice of one beam and the opposite of the other beam,
  // after their boost in X
  ///////////////////////////////
  // Rotation for the CS Frame

  TRotation rotationCS;

  TLorentzVector beaminX(beam);
  TLorentzVector targinX(targ);
  targinX.Boost(boostX);
  beaminX.Boost(boostX);

  //Bisectrice: sum of unit vectors (remember: you need to invert one beam vector)
  TVector3 beam_targ_bisecinX((beaminX.Vect().Unit() - targinX.Vect().Unit()).Unit());

  // Define a rotationCS Matrix, with Z along the bisectric, 
  TVector3 newZaxisCS(beam_targ_bisecinX.Unit());
  TVector3 newYaxisCS(beaminX.Vect().Unit().Cross(newZaxisCS).Unit());
  TVector3 newXaxisCS(newYaxisCS.Unit().Cross(newZaxisCS).Unit());
  rotationCS.RotateAxes(newXaxisCS, newYaxisCS, newZaxisCS);
  rotationCS.Invert();

  //// costhetastar
  TLorentzVector thep4Z1inXFrame_rotCS(p4Z1);
  TLorentzVector thep4Z2inXFrame_rotCS(p4Z2);
  thep4Z1inXFrame_rotCS.Transform(rotationCS);
  thep4Z2inXFrame_rotCS.Transform(rotationCS);
  thep4Z1inXFrame_rotCS.Boost(boostX);
  thep4Z2inXFrame_rotCS.Boost(boostX);
  TVector3 theZ1XrotCS_p3 = TVector3(thep4Z1inXFrame_rotCS.X(), thep4Z1inXFrame_rotCS.Y(), thep4Z1inXFrame_rotCS.Z());
  costhetastar = theZ1XrotCS_p3.CosTheta();

  //// --------------------------- Phi and Phi1 (old phistar1 - azimuthal production angle)
  //    TVector3 boostX = -(thep4H.BoostVector());
  TLorentzVector p4M11_BX_rotCS(p4M11);
  TLorentzVector p4M12_BX_rotCS(p4M12);
  TLorentzVector p4M21_BX_rotCS(p4M21);
  TLorentzVector p4M22_BX_rotCS(p4M22);
  p4M11_BX_rotCS.Transform(rotationCS);
  p4M12_BX_rotCS.Transform(rotationCS);
  p4M21_BX_rotCS.Transform(rotationCS);
  p4M22_BX_rotCS.Transform(rotationCS);
  p4M11_BX_rotCS.Boost(boostX);
  p4M12_BX_rotCS.Boost(boostX);
  p4M21_BX_rotCS.Boost(boostX);
  p4M22_BX_rotCS.Boost(boostX);

  TLorentzVector p4Z1_BX_rotCS = p4M11_BX_rotCS + p4M12_BX_rotCS;
  TVector3 p3V1_BX_rotCS = (p4Z1_BX_rotCS.Vect()).Unit();
  TVector3 beamAxis(0, 0, 1);
  TVector3 normal1_BX_rotCS = (p4M11_BX_rotCS.Vect().Cross(p4M12_BX_rotCS.Vect())).Unit();
  TVector3 normal2_BX_rotCS = (p4M21_BX_rotCS.Vect().Cross(p4M22_BX_rotCS.Vect())).Unit();
  TVector3 normalSC_BX_rotCS = (beamAxis.Cross(p3V1_BX_rotCS)).Unit();

  //// Phi
  float tmpSgnPhi = p3V1_BX_rotCS.Dot(normal1_BX_rotCS.Cross(normal2_BX_rotCS));
  float sgnPhi = 0;
  if (fabs(tmpSgnPhi)>0.) sgnPhi = tmpSgnPhi/fabs(tmpSgnPhi);
  float dot_BX12 = normal1_BX_rotCS.Dot(normal2_BX_rotCS);
  if (fabs(dot_BX12)>=1.) dot_BX12 *= 1./fabs(dot_BX12);
  Phi = sgnPhi * acos(-1.*dot_BX12);

  //// Phi1
  float tmpSgnPhi1 = p3V1_BX_rotCS.Dot(normal1_BX_rotCS.Cross(normalSC_BX_rotCS));
  float sgnPhi1 = 0;
  if (fabs(tmpSgnPhi1)>0.) sgnPhi1 = tmpSgnPhi1/fabs(tmpSgnPhi1);
  float dot_BX1SC = normal1_BX_rotCS.Dot(normalSC_BX_rotCS);
  if (fabs(dot_BX1SC)>=1.) dot_BX1SC *= 1./fabs(dot_BX1SC);
  Phi1 = sgnPhi1 * acos(dot_BX1SC);

  //// --------------------------- costheta1
  if (!(fabs(Z1_lept1Id)==21 || fabs(Z1_lept1Id)==22 || fabs(Z1_lept2Id)==21 || fabs(Z1_lept2Id)==22)){
    //define Z1 rotation 
    TRotation rotationZ1;
    TVector3 newZaxisZ1(thep4Z1inXFrame_rotCS.Vect().Unit());
    TVector3 newXaxisZ1(newYaxisCS.Cross(newZaxisZ1).Unit());
    TVector3 newYaxisZ1(newZaxisZ1.Cross(newXaxisZ1).Unit());
    rotationZ1.RotateAxes(newXaxisZ1, newYaxisZ1, newZaxisZ1);
    rotationZ1.Invert();

    TLorentzVector thep4Z1inXFrame_rotCS_rotZ1(thep4Z1inXFrame_rotCS);
    thep4Z1inXFrame_rotCS_rotZ1.Transform(rotationZ1);
    TVector3 boostZ1inX_rotCS_rotZ1= -(thep4Z1inXFrame_rotCS_rotZ1.BoostVector());

    TLorentzVector p4M11_BX_rotCS_rotZ1(p4M11_BX_rotCS);
    TLorentzVector p4M12_BX_rotCS_rotZ1(p4M12_BX_rotCS);
    TLorentzVector p4M21_BX_rotCS_rotZ1(p4M21_BX_rotCS);
    TLorentzVector p4M22_BX_rotCS_rotZ1(p4M22_BX_rotCS);
    p4M11_BX_rotCS_rotZ1.Transform(rotationZ1);
    p4M12_BX_rotCS_rotZ1.Transform(rotationZ1);
    p4M21_BX_rotCS_rotZ1.Transform(rotationZ1);
    p4M22_BX_rotCS_rotZ1.Transform(rotationZ1);
    p4M11_BX_rotCS_rotZ1.Boost(boostZ1inX_rotCS_rotZ1);
    p4M12_BX_rotCS_rotZ1.Boost(boostZ1inX_rotCS_rotZ1);
    p4M21_BX_rotCS_rotZ1.Boost(boostZ1inX_rotCS_rotZ1);
    p4M22_BX_rotCS_rotZ1.Boost(boostZ1inX_rotCS_rotZ1);

    TLorentzVector p4V2_BX_rotCS_rotZ1 = p4M21_BX_rotCS_rotZ1 + p4M22_BX_rotCS_rotZ1;
    //// costheta1
    costheta1 = -p4V2_BX_rotCS_rotZ1.Vect().Dot(p4M11_BX_rotCS_rotZ1.Vect())/p4V2_BX_rotCS_rotZ1.Vect().Mag()/p4M11_BX_rotCS_rotZ1.Vect().Mag();
  }
  else costheta1=0;

  //// --------------------------- costheta2
  if (!(fabs(Z2_lept1Id)==21 || fabs(Z2_lept1Id)==22 || fabs(Z2_lept2Id)==21 || fabs(Z2_lept2Id)==22)){
    //define Z2 rotation 
    TRotation rotationZ2;
    TVector3 newZaxisZ2(thep4Z2inXFrame_rotCS.Vect().Unit());
    TVector3 newXaxisZ2(newYaxisCS.Cross(newZaxisZ2).Unit());
    TVector3 newYaxisZ2(newZaxisZ2.Cross(newXaxisZ2).Unit());
    rotationZ2.RotateAxes(newXaxisZ2, newYaxisZ2, newZaxisZ2);
    rotationZ2.Invert();

    TLorentzVector thep4Z2inXFrame_rotCS_rotZ2(thep4Z2inXFrame_rotCS);
    thep4Z2inXFrame_rotCS_rotZ2.Transform(rotationZ2);
    TVector3 boostZ2inX_rotCS_rotZ2= -(thep4Z2inXFrame_rotCS_rotZ2.BoostVector());

    TLorentzVector p4M11_BX_rotCS_rotZ2(p4M11_BX_rotCS);
    TLorentzVector p4M12_BX_rotCS_rotZ2(p4M12_BX_rotCS);
    TLorentzVector p4M21_BX_rotCS_rotZ2(p4M21_BX_rotCS);
    TLorentzVector p4M22_BX_rotCS_rotZ2(p4M22_BX_rotCS);
    p4M11_BX_rotCS_rotZ2.Transform(rotationZ2);
    p4M12_BX_rotCS_rotZ2.Transform(rotationZ2);
    p4M21_BX_rotCS_rotZ2.Transform(rotationZ2);
    p4M22_BX_rotCS_rotZ2.Transform(rotationZ2);
    p4M11_BX_rotCS_rotZ2.Boost(boostZ2inX_rotCS_rotZ2);
    p4M12_BX_rotCS_rotZ2.Boost(boostZ2inX_rotCS_rotZ2);
    p4M21_BX_rotCS_rotZ2.Boost(boostZ2inX_rotCS_rotZ2);
    p4M22_BX_rotCS_rotZ2.Boost(boostZ2inX_rotCS_rotZ2);


    TLorentzVector p4V1_BX_rotCS_rotZ2= p4M11_BX_rotCS_rotZ2 + p4M12_BX_rotCS_rotZ2;
    //// costheta2
    costheta2 = -p4V1_BX_rotCS_rotZ2.Vect().Dot(p4M21_BX_rotCS_rotZ2.Vect())/p4V1_BX_rotCS_rotZ2.Vect().Mag()/p4M21_BX_rotCS_rotZ2.Vect().Mag();
  }
  else costheta2=0;

  if (isnan(costhetastar) || isnan(costheta1) || isnan(costheta2) || isnan(Phi) || isnan(Phi1)){
    cout << "WARNING: NaN in computeAngles: "
      << costhetastar << " "
      << costheta1  << " "
      << costheta2  << " "
      << Phi  << " "
      << Phi1  << " " << endl;
  }
}

/***** Associated production *****/
void TUtil::computeVBFangles(
  float& costhetastar,
  float& costheta1,
  float& costheta2,
  float& Phi,
  float& Phi1,
  float& Q2V1,
  float& Q2V2,
  TLorentzVector p4M11, int Z1_lept1Id,
  TLorentzVector p4M12, int Z1_lept2Id,
  TLorentzVector p4M21, int Z2_lept1Id,
  TLorentzVector p4M22, int Z2_lept2Id,
  TLorentzVector jet1, int jet1Id,
  TLorentzVector jet2, int jet2Id,
  TLorentzVector* injet1, int injet1Id, // Gen. partons in lab frame
  TLorentzVector* injet2, int injet2Id
  ){
  TLorentzVector nullFourVector(0, 0, 0, 0);
  if (p4M12==nullFourVector || p4M22==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f13Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M21, Z2_lept1Id);
    p4M11 = f13Pair.first;
    p4M21 = f13Pair.second;
  }
  else if (p4M11==nullFourVector || p4M21==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f24Pair = TUtil::removeMassFromPair(p4M12, Z1_lept2Id, p4M22, Z2_lept2Id);
    p4M12 = f24Pair.first;
    p4M22 = f24Pair.second;
  }
  else{
    pair<TLorentzVector, TLorentzVector> f12Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M12, Z1_lept2Id);
    pair<TLorentzVector, TLorentzVector> f34Pair = TUtil::removeMassFromPair(p4M21, Z2_lept1Id, p4M22, Z2_lept2Id);
    p4M11 = f12Pair.first;
    p4M12 = f12Pair.second;
    p4M21 = f34Pair.first;
    p4M22 = f34Pair.second;
  }

  TLorentzVector jet1massless, jet2massless;
  pair<TLorentzVector, TLorentzVector> jetPair = TUtil::removeMassFromPair(jet1, jet1Id, jet2, jet2Id);
  jet1massless = jetPair.first;
  jet2massless = jetPair.second;

  //build Z 4-vectors
  TLorentzVector p4Z1 = p4M11 + p4M12;
  TLorentzVector p4Z2 = p4M21 + p4M22;
  TLorentzVector pH = p4Z1+p4Z2;
  //jet1 is defined as going forwards (bigger pz), jet2 going backwards (smaller pz)
  if (jet1massless.Z() < jet2massless.Z()) { swap(jet1massless, jet2massless); swap(jet1Id, jet2Id); }

  //Find the incoming partons - first boost so the pT(HJJ) = 0, then boost away the pz.
  //This preserves the z direction.  Then assume that the partons come in this z direction.
  //This is exactly correct at LO (since pT=0 anyway).
  //Then associate the one going forwards with jet1 and the one going backwards with jet2
  TLorentzRotation movingframe;
  TLorentzVector pHJJ = pH+jet1massless+jet2massless;
  TLorentzVector pHJJ_perp(pHJJ.X(), pHJJ.Y(), 0, pHJJ.T());
  movingframe.Boost(-pHJJ_perp.BoostVector());
  pHJJ.Boost(-pHJJ_perp.BoostVector());
  movingframe.Boost(-pHJJ.BoostVector());
  pHJJ.Boost(-pHJJ.BoostVector());   //make sure to boost HJJ AFTER boosting movingframe

  TLorentzVector P1(0, 0, pHJJ.T()/2, pHJJ.T()/2);
  TLorentzVector P2(0, 0, -pHJJ.T()/2, pHJJ.T()/2);
  // Transform incoming partons back to original frame
  P1.Transform(movingframe.Inverse());
  P2.Transform(movingframe.Inverse());
  //movingframe, HJJ, and HJJ_T will not be used anymore
  if (injet1!=0 && injet2!=0){ // Handle gen. partons if they are available
    if (fabs((*injet1+*injet2).P()-pHJJ.P())<pHJJ.P()*1e-4){
      P1=*injet1;
      P2=*injet2;
      if (P1.Z() < P2.Z()){
        swap(P1, P2);
        swap(injet1Id, injet2Id);
      }
      // In the case of gen. partons, check if the intermediates are a Z or a W.
      int diff1Id = jet1Id-injet1Id;
      int diff2Id = jet2Id-injet2Id;
      if (
        !(
        (diff1Id==0 && diff2Id==0 && !(injet1Id==21 || injet2Id==21)) // Two Z bosons
        ||
        ((fabs(diff1Id)==1 || fabs(diff1Id)==3 || fabs(diff1Id)==5) && (fabs(diff2Id)==1 || fabs(diff2Id)==3 || fabs(diff2Id)==5)) // Two W bosons, do not check W+ vs W-
        )
        ){
        int diff12Id = jet1Id-injet2Id;
        int diff21Id = jet2Id-injet1Id;
        if (
          ((diff12Id==0 || diff21Id==0) && !(injet1Id==21 || injet2Id==21)) // At least one Z boson
          ||
          ((fabs(diff12Id)==1 || fabs(diff12Id)==3 || fabs(diff12Id)==5) || (fabs(diff21Id)==1 || fabs(diff21Id)==3 || fabs(diff21Id)==5)) // At least one W boson
          ){
          swap(P1, P2);
          swap(injet1Id, injet2Id);
        }
      }
    }
  }

  TLorentzRotation ZZframe;
  ZZframe.Boost(-pH.BoostVector());
  P1.Transform(ZZframe);
  P2.Transform(ZZframe);
  p4Z1.Transform(ZZframe);
  p4Z2.Transform(ZZframe);
  jet1massless.Transform(ZZframe);
  jet2massless.Transform(ZZframe);

  TLorentzVector V1 = P1-jet1massless; // V1 = (-p12) - p11 = -Z1
  TLorentzVector V2 = P2-jet2massless; // V2 = (-p22) - p21 = -Z2
  Q2V1 = -V1.M2();
  Q2V2 = -V2.M2();

  costhetastar = -V1.Vect().Unit().Dot(p4Z2.Vect().Unit());
  costheta1 = -V1.Vect().Unit().Dot(jet1massless.Vect().Unit());
  costheta2 = -V2.Vect().Unit().Dot(jet2massless.Vect().Unit());

  TVector3 normvec1 = P1.Vect().Cross(jet1massless.Vect()).Unit(); // p11 x p12 = (-p12) x p11
  TVector3 normvec2 = P2.Vect().Cross(jet2massless.Vect()).Unit(); // p21 x p22 = (-p22) x p21
  TVector3 normvec3 = V1.Vect().Cross(p4Z2.Vect()).Unit(); // == z x Z1

  double cosPhi = normvec1.Dot(normvec2);
  double sgnPhi = normvec1.Cross(normvec2).Dot(-V1.Vect());
  double cosPhi1 = normvec1.Dot(normvec3);
  double sgnPhi1 = normvec1.Cross(normvec3).Dot(-V1.Vect());
  if (fabs(cosPhi)>1) cosPhi *= 1./fabs(cosPhi);
  if (fabs(cosPhi1)>1) cosPhi1 *= 1./fabs(cosPhi1);
  Phi = TMath::Sign(acos(-cosPhi), sgnPhi);            //TMath::Sign(a,b) = |a|*(b/|b|)
  Phi1 = TMath::Sign(acos(cosPhi1), sgnPhi1);
}
void TUtil::computeVHangles(
  float& costhetastar,
  float& costheta1,
  float& costheta2,
  float& Phi,
  float& Phi1,
  TLorentzVector p4M11, int Z1_lept1Id,
  TLorentzVector p4M12, int Z1_lept2Id,
  TLorentzVector p4M21, int Z2_lept1Id,
  TLorentzVector p4M22, int Z2_lept2Id,
  TLorentzVector jet1, int jet1Id,
  TLorentzVector jet2, int jet2Id,
  TLorentzVector* injet1, int injet1Id, // Gen. partons in lab frame
  TLorentzVector* injet2, int injet2Id
  ){
  TLorentzVector nullFourVector(0, 0, 0, 0);
  if (p4M12==nullFourVector || p4M22==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f13Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M21, Z2_lept1Id);
    p4M11 = f13Pair.first;
    p4M21 = f13Pair.second;
  }
  else if (p4M11==nullFourVector || p4M21==nullFourVector){
    pair<TLorentzVector, TLorentzVector> f24Pair = TUtil::removeMassFromPair(p4M12, Z1_lept2Id, p4M22, Z2_lept2Id);
    p4M12 = f24Pair.first;
    p4M22 = f24Pair.second;
  }
  else{
    pair<TLorentzVector, TLorentzVector> f12Pair = TUtil::removeMassFromPair(p4M11, Z1_lept1Id, p4M12, Z1_lept2Id);
    pair<TLorentzVector, TLorentzVector> f34Pair = TUtil::removeMassFromPair(p4M21, Z2_lept1Id, p4M22, Z2_lept2Id);
    p4M11 = f12Pair.first;
    p4M12 = f12Pair.second;
    p4M21 = f34Pair.first;
    p4M22 = f34Pair.second;
  }

  TLorentzVector jet1massless, jet2massless;
  pair<TLorentzVector, TLorentzVector> jetPair = TUtil::removeMassFromPair(jet1, jet1Id, jet2, jet2Id);
  jet1massless = jetPair.first;
  jet2massless = jetPair.second;

  // Build Z 4-vectors
  TLorentzVector p4Z1 = p4M11 + p4M12;
  TLorentzVector p4Z2 = p4M21 + p4M22;
  TLorentzVector pH = p4Z1 + p4Z2;

  // Apply convention for outgoing particles
  if (
    (jet1Id*jet2Id<0 && jet1Id<0) // for OS pairs: jet1 must be the particle
    ||
    (jet1Id*jet2Id>0 && jet1massless.Phi()<=jet2massless.Phi()) // for SS pairs: use random deterministic convention
    ){
    swap(jet1massless, jet2massless);
    swap(jet1Id, jet2Id);
  }

  //Find the incoming partons - first boost so the pT(HJJ) = 0, then boost away the pz.
  //This preserves the z direction.  Then assume that the partons come in this z direction.
  //This is exactly correct at LO (since pT=0 anyway).
  //Then associate the one going forwards with jet1 and the one going backwards with jet2
  TLorentzRotation movingframe;
  TLorentzVector pHJJ = pH+jet1massless+jet2massless;
  TLorentzVector pHJJ_perp(pHJJ.X(), pHJJ.Y(), 0, pHJJ.T());
  movingframe.Boost(-pHJJ_perp.BoostVector());
  pHJJ.Boost(-pHJJ_perp.BoostVector());
  movingframe.Boost(-pHJJ.BoostVector());
  pHJJ.Boost(-pHJJ.BoostVector());   //make sure to boost HJJ AFTER boosting movingframe

  TLorentzVector P1(0, 0, -pHJJ.T()/2, pHJJ.T()/2);
  TLorentzVector P2(0, 0, pHJJ.T()/2, pHJJ.T()/2);
  // Transform incoming partons back to the original frame
  P1.Transform(movingframe.Inverse());
  P2.Transform(movingframe.Inverse());
  //movingframe, HJJ, and HJJ_T will not be used anymore
  if (injet1!=0 && injet2!=0){ // Handle gen. partons if they are available
    if (fabs((*injet1+*injet2).P()-pHJJ.P())<=pHJJ.P()*1e-4){
      P1=*injet1;
      P2=*injet2;
      // Apply convention for incoming (!) particles
      if (
        (injet1Id*injet2Id<0 && injet1Id>0) // for OS pairs: parton 2 must be the particle
        ||
        (injet1Id*injet2Id>0 && P1.Z()>=P2.Z()) //for SS pairs: use random deterministic convention
        ){
        swap(P1, P2);
        swap(injet1Id, injet2Id);
      }
    }
  }

  // Rotate every vector such that Z1 - Z2 axis is the "beam axis" analogue of decay
  TLorentzRotation ZZframe;
  TVector3 beamAxis(0, 0, 1);
  if (p4Z1==nullFourVector || p4Z2==nullFourVector){
    TVector3 pNewAxis = (p4Z2-p4Z1).Vect().Unit(); // Let Z2 be in the z direction so that once the direction of H is reversed, Z1 is in the z direction
    if (pNewAxis != nullFourVector.Vect()){
      TVector3 pNewAxisPerp = pNewAxis.Cross(beamAxis);
      ZZframe.Rotate(acos(pNewAxis.Dot(beamAxis)), pNewAxisPerp);

      P1.Transform(ZZframe);
      P2.Transform(ZZframe);
      jet1massless = -jet1massless;
      jet2massless = -jet2massless;
      jet1massless.Transform(ZZframe);
      jet2massless.Transform(ZZframe);
      jet1massless = -jet1massless;
      jet2massless = -jet2massless;
    }
  }
  else{
    ZZframe.Boost(-pH.BoostVector());
    p4Z1.Boost(-pH.BoostVector());
    p4Z2.Boost(-pH.BoostVector());
    TVector3 pNewAxis = (p4Z2-p4Z1).Vect().Unit(); // Let Z2 be in the z direction so that once the direction of H is reversed, Z1 is in the z direction
    TVector3 pNewAxisPerp = pNewAxis.Cross(beamAxis);
    ZZframe.Rotate(acos(pNewAxis.Dot(beamAxis)), pNewAxisPerp);
    P1.Transform(ZZframe);
    P2.Transform(ZZframe);
    jet1massless = -jet1massless;
    jet2massless = -jet2massless;
    jet1massless.Transform(ZZframe);
    jet2massless.Transform(ZZframe);
    jet1massless = -jet1massless;
    jet2massless = -jet2massless;
  }

  TUtil::computeAngles(
    -P1, 23, // Id is 23 to avoid an attempt to remove quark mass
    -P2, 0, // Id is 0 to avoid swapping
    jet1massless, 23,
    jet2massless, 0,
    costhetastar,
    costheta1,
    costheta2,
    Phi,
    Phi1
    );
}


/****************************************************/
/***** JHUGen- and MCFM-related ME computations *****/
/****************************************************/

void TUtil::SetEwkCouplingParameters(){
  ewscheme_.ewscheme = 3; // Switch ewscheme to full control, default is 1

/*
// Settings used in Run I MC, un-synchronized to JHUGen and Run II generation
  ewinput_.Gf_inp = 1.16639E-05;
  ewinput_.wmass_inp = 80.385;
  ewinput_.zmass_inp = 91.1876;
//  ewinput_.aemmz_inp = 7.81751e-3; // Not used in the compiled new default MCFM ewcheme=1
//  ewinput_.xw_inp = 0.23116864; // Not used in the compiled new default MCFM ewcheme=1
  ewinput_.aemmz_inp = 7.562468901984759e-3;
  ewinput_.xw_inp = 0.2228972225239183;
*/

// SETTINGS TO MATCH JHUGen ME/generator:
  ewinput_.Gf_inp=1.16639E-05;
  ewinput_.aemmz_inp=1./128.;
  ewinput_.wmass_inp=80.399;
  ewinput_.zmass_inp=91.1876;
  ewinput_.xw_inp=0.23119;

/*
// INPUT SETTINGS in default MCFM generator:
  ewscheme_.ewscheme = 1;
  ewinput_.Gf_inp=1.16639E-05;
  ewinput_.wmass_inp=80.398;
  ewinput_.zmass_inp=91.1876;
  ewinput_.aemmz_inp=7.7585538055706e-3;
  ewinput_.xw_inp=0.2312;
*/
/*
// ACTUAL SETTINGS in default MCFM generator, gives the same values as above but is more explicit:
  ewscheme_.ewscheme = 3;
  ewinput_.Gf_inp=1.16639E-05;
  ewinput_.wmass_inp=80.398;
  ewinput_.zmass_inp=91.1876;
  ewinput_.aemmz_inp=7.55638390728736e-3;
  ewinput_.xw_inp=0.22264585341299625;
*/
}
double TUtil::InterpretScaleScheme(TVar::Production production, TVar::MatrixElement matrixElement, TVar::EventScaleScheme scheme, TLorentzVector p[mxpart]){
  double Q=0;
  TLorentzVector nullFourVector(0, 0, 0, 0);
  if (scheme == TVar::Fixed_mH) Q = masses_mcfm_.hmass;
  else if (scheme == TVar::Fixed_mW) Q = masses_mcfm_.wmass;
  else if (scheme == TVar::Fixed_mZ) Q = masses_mcfm_.zmass;
  else if (scheme == TVar::Fixed_mWPlusmH) Q = (masses_mcfm_.wmass+masses_mcfm_.hmass);
  else if (scheme == TVar::Fixed_mZPlusmH) Q = (masses_mcfm_.zmass+masses_mcfm_.hmass);
  else if (scheme == TVar::Fixed_TwomtPlusmH) Q = (2.*masses_mcfm_.mt+masses_mcfm_.hmass);
  else if (scheme == TVar::Fixed_mtPlusmH) Q = masses_mcfm_.mt+masses_mcfm_.hmass;
  else if (scheme == TVar::Dynamic_qH){
    TLorentzVector pTotal = p[2]+p[3]+p[4]+p[5];
    Q = fabs(pTotal.M());
  }
  else if (scheme == TVar::Dynamic_qJJH){
    TLorentzVector pTotal = p[2]+p[3]+p[4]+p[5]+p[6]+p[7];
    Q = fabs(pTotal.M());
  }
  else if (scheme == TVar::Dynamic_qJJ_qH){
    TLorentzVector qH = p[2]+p[3]+p[4]+p[5];
    TLorentzVector qJJ = p[6]+p[7];
    Q = fabs(qH.M()+qJJ.M());
  }
  else if (scheme == TVar::Dynamic_qJ_qJ_qH){
    TLorentzVector qH = p[2]+p[3]+p[4]+p[5];
    Q = fabs(qH.M()+p[6].M()+p[7].M());
  }
  else if (scheme == TVar::Dynamic_HT){
    for (int c=2; c<mxpart; c++) Q += p[c].Pt(); // Scalar sum of all pTs
  }
  else if (scheme == TVar::DefaultScaleScheme){
    if (matrixElement==TVar::JHUGen){
      if (
        production==TVar::JJGG
        || production==TVar::JJVBF
        || production==TVar::JH
        || production==TVar::ZZGG
        || production==TVar::ZZQQB
        || production==TVar::ZZQQB_STU
        || production==TVar::ZZQQB_S
        || production==TVar::ZZQQB_TU
        || production==TVar::ZZINDEPENDENT
        || production==TVar::Lep_WH || production==TVar::Had_WH
        || production==TVar::Lep_ZH || production==TVar::Had_ZH
        || production==TVar::GammaH
        ){
        TLorentzVector pTotal = p[2]+p[3]+p[4]+p[5];
        Q = fabs(pTotal.M());
      }
      else if (production==TVar::ttH || production==TVar::bbH) Q = (2.*masses_mcfm_.mt+masses_mcfm_.hmass);
    }
    else if (matrixElement==TVar::MCFM){
      if (
        production==TVar::JJGG
        || production==TVar::JJVBF
        || production==TVar::JH
        || production==TVar::ZZGG
        || production==TVar::ZZQQB
        || production==TVar::ZZQQB_STU
        || production==TVar::ZZQQB_S
        || production==TVar::ZZQQB_TU
        || production==TVar::ZZINDEPENDENT
        || production==TVar::Lep_WH || production==TVar::Had_WH
        || production==TVar::Lep_ZH || production==TVar::Had_ZH
        || production==TVar::GammaH
        ){
        TLorentzVector pTotal = p[2]+p[3]+p[4]+p[5];
        Q = fabs(pTotal.M());
      }
      else if (
        production==TVar::ttH
        || production==TVar::bbH
        ){
        TLorentzVector pTotal = p[2]+p[3]+p[4]+p[5]+p[6]+p[7];
        Q = fabs(pTotal.M());
      }
    }
  }

  if (Q<=0.){
    cout << "Scaling fails, defaulting to dynamic scheme m3456 " << endl;
    TLorentzVector pTotal = p[2]+p[3]+p[4]+p[5];
    Q = fabs(pTotal.M());
  }
  return Q;
}
void TUtil::SetAlphaS(double Q_ren, double Q_fac, double multiplier_ren, double multiplier_fac, int mynloop, int mynflav, string mypartons){
  bool hasReset=false;
  if (multiplier_ren<=0. || multiplier_fac<=0.){
    cerr << "TUtil::SetAlphaS: Invalid scale multipliers" << endl;
    return;
  }
  if (Q_ren<=1. || Q_fac<=1. || mynloop<=0 || mypartons.compare("Default")==0){
    if (Q_ren<0.) cout << "TUtil::SetAlphaS: Invalid QCD scale for alpha_s, setting to mH/2..." << endl;
    if (Q_fac<0.) cout << "TUtil::SetAlphaS: Invalid factorization scale, setting to mH/2..." << endl;
    Q_ren = (masses_mcfm_.hmass)*0.5;
    Q_fac = Q_ren;
    mynloop = 1;
    hasReset=true;
  }
  if (!hasReset){
    Q_ren *= multiplier_ren;
    Q_fac *= multiplier_fac;
  }

  /***** JHUGen Alpha_S *****/
  // To be implemented

  /***** MCFM Alpha_S *****/
  bool nflav_is_same = (nflav_.nflav == mynflav);
  if (!nflav_is_same) cout << "TUtil::SetAlphaS: nflav=" << nflav_.nflav << " is the only one supported." << endl;
  scale_.scale = Q_ren;
  scale_.musq = Q_ren*Q_ren;
  facscale_.facscale = Q_fac;
  if (mynloop!=1){
    cout << "TUtil::SetAlphaS: Only nloop=1 is supported!" << endl;
    mynloop=1;
  }
  nlooprun_.nlooprun = mynloop;

  /*
  ///// Disabling alpha_s computation from MCFM to replace with the JHUGen implementation, allows LHAPDF interface readily /////
  // For proper pdfwrapper_linux.f execution (alpha_s computation does not use pdf but examines the pdf name to initialize amz.)
  if (mypartons.compare("Default")!=0 && mypartons.compare("cteq6_l")!=0 && mypartons.compare("cteq6l1")!=0){
  cout << "Only default=cteq6l1 or cteq6_l are supported. Modify mela.cc symlinks, put the pdf table into data/Pdfdata and retry. Setting mypartons to Default..." << endl;
  mypartons = "Default";
  }
  // From pdfwrapper_linux.f:
  if (mypartons.compare("cteq6_l")==0) couple_.amz = 0.118;
  else if (mypartons.compare("cteq6l1")==0 || mypartons.compare("Default")==0) couple_.amz = 0.130;
  else couple_.amz = 0.118; // Add pdf as appropriate

  if (!nflav_is_same){
    nflav_.nflav = mynflav;

    if (mypartons.compare("Default")!=0) sprintf(pdlabel_.pdlabel, "%s", mypartons.c_str());
    else sprintf(pdlabel_.pdlabel, "%s", "cteq6l1"); // Default pdf is cteq6l1
    coupling2_();
  }
  else qcdcouple_.as = alphas_(&(scale_.scale), &(couple_.amz), &(nlooprun_.nlooprun));
  */

  const double GeV=1./100.;
  double muren_jhu = scale_.scale*GeV;
  double mufac_jhu = facscale_.facscale*GeV;
  __modjhugenmela_MOD_setmurenfac(&muren_jhu, &mufac_jhu);
  __modkinematics_MOD_evalalphas();
  __modjhugenmela_MOD_getalphasalphasmz(&(qcdcouple_.as), &(couple_.amz));

  qcdcouple_.gsq = 4.0*TMath::Pi()*qcdcouple_.as;
  qcdcouple_.ason2pi = qcdcouple_.as/(2.0*TMath::Pi());
  qcdcouple_.ason4pi = qcdcouple_.as/(4.0*TMath::Pi());

  // TEST RUNNING SCALE PER EVENT:
  /*
  if(verbosity >= TVar::DEBUG){
  cout << "My pdf is: " << pdlabel_.pdlabel << endl;
  cout << "My Q_ren: " << Q_ren << " | My alpha_s: " << qcdcouple_.as << " at order " << nlooprun_.nlooprun << " with a(m_Z): " << couple_.amz << '\t'
  << "Nflav: " << nflav_.nflav << endl;
  */
}
bool TUtil::MCFM_chooser(TVar::Process process, TVar::Production production, TVar::LeptonInterference leptonInterf, MELACandidate* cand){
  if (cand==0){ cerr << "TUtil::MCFM_chooser: Invalid candidate!" << endl; return false; } // Invalid candidate
  MELAParticle* V1 = cand->getSortedV(0);
  MELAParticle* V2 = cand->getSortedV(1);
  if (V1==0 || V2==0){ cerr << "TUtil::MCFM_chooser: Invalid candidate Vs:" << V1 << '\t' << V2 << endl; return false; } // Invalid candidate Vs

  // LEFT HERE
  // FICME:     else return false always has quarks missing. Add qcdjets +=2 and zcouple_.l1r1/l2r2=zcouple_.l[iq]/r[iq]


  unsigned int ndau = V1->getNDaughters() + V2->getNDaughters();
  bool definiteInterf=(
    ndau>3
    &&
    abs(V1->getDaughter(0)->id)==abs(V2->getDaughter(0)->id)
    &&
    abs(V1->getDaughter(1)->id)==abs(V2->getDaughter(1)->id)
    &&
    !PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) && !PDGHelpers::isAnUnknownJet(V1->getDaughter(1)->id)
    );
  // Determine if the decay mode involves WW or ZZ, to be used for ZZ or WW-specific signal MEs
  //bool isZG = PDGHelpers::isAZBoson(V1->id) && PDGHelpers::isAPhoton(V2->id);
  bool isWW = PDGHelpers::isAWBoson(V1->id) && PDGHelpers::isAWBoson(V2->id);
  bool isZZ = PDGHelpers::isAZBoson(V1->id) && PDGHelpers::isAZBoson(V2->id);

  sprintf(runstring_.runstring, "test");
  if (
    ndau>=4
    &&
    (
    ((production == TVar::ZZQQB_STU || production == TVar::ZZQQB_S || production == TVar::ZZQQB_TU) && process == TVar::bkgZZ)
    ||
    ((production == TVar::ZZINDEPENDENT || production == TVar::ZZQQB) && process == TVar::bkgZZ)
    )
    ){
    //81 '  f(p1)+f(p2) --> Z^0(-->mu^-(p3)+mu^+(p4)) + Z^0(-->e^-(p5)+e^+(p6))'
    //86 '  f(p1)+f(p2) --> Z^0(-->e^-(p5)+e^+(p6))+Z^0(-->mu^-(p3)+mu^+(p4)) (NO GAMMA*)'
    //90 '  f(p1)+f(p2) --> Z^0(-->e^-(p3)+e^+(p4)) + Z^0(-->e^-(p5)+e^+(p6))' 'L'
    npart_.npart=4;
    nqcdjets_.nqcdjets=0;

    nwz_.nwz=0;
    bveg1_mcfm_.ndim=10;
    breit_.n2=1;
    breit_.n3=1;

    breit_.mass2=masses_mcfm_.zmass;
    breit_.width2=masses_mcfm_.zwidth;
    breit_.mass3=masses_mcfm_.zmass;
    breit_.width3=masses_mcfm_.zwidth;

    if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
      zcouple_.q1=-1.0;
      zcouple_.l1=zcouple_.le;
      zcouple_.r1=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
      zcouple_.q1=0;
      zcouple_.l1=zcouple_.ln;
      zcouple_.r1=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q1=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l1=zcouple_.l[jetid];
        zcouple_.r1=zcouple_.r[jetid];
      }
      else{
        zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    if (PDGHelpers::isALepton(V2->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(1)->id)){
      zcouple_.q2=-1.0;
      zcouple_.l2=zcouple_.le;
      zcouple_.r2=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V2->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(1)->id)){
      zcouple_.q2=0;
      zcouple_.l2=zcouple_.ln;
      zcouple_.r2=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V2->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q2=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V2->getDaughter(0)->id) ? abs(V2->getDaughter(1)->id) : abs(V2->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l2=zcouple_.l[jetid];
        zcouple_.r2=zcouple_.r[jetid];
      }
      else{
        zcouple_.l2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    vsymfact_.vsymfact=1.0;
    interference_.interference=false;
    if (definiteInterf && (leptonInterf==TVar::DefaultLeptonInterf || leptonInterf==TVar::InterfOn)){
      //90 '  f(p1)+f(p2) --> Z^0(-->e^-(p3)+e^+(p4)) + Z^0(-->e^-(p5)+e^+(p6))' 'L'
      vsymfact_.vsymfact=0.125; // MELA FACTOR (0.25 in MCFM 6.8)  --->   Result of just removing if(bw34_56) statements in FORTRAN code and not doing anything else
      //vsymfact_.vsymfact=0.25; // MELA FACTOR (Same 0.25 in MCFM 6.7)
      interference_.interference=true;
    }
  }
  else if (
    ndau>=4
    &&
    ((production == TVar::ZZINDEPENDENT || production == TVar::ZZQQB) && process == TVar::bkgWW)
    ){
    // Processes 61 (4l), 62 (2l2q), 64 (2q2l)

    nqcdjets_.nqcdjets=0;
    nwz_.nwz=1;
    bveg1_mcfm_.ndim=10;
    breit_.n2=1;
    breit_.n3=1;
    breit_.mass2=masses_mcfm_.wmass;
    breit_.width2=masses_mcfm_.wwidth;
    breit_.mass3=masses_mcfm_.wmass;
    breit_.width3=masses_mcfm_.wwidth;
    srdiags_.srdiags=false;
    sprintf((plabel_.plabel)[6], "pp");
    zcouple_.l1=1.;

    if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id)){
      sprintf((plabel_.plabel)[2], "nl");
      sprintf((plabel_.plabel)[3], "ea");
    }
    else if (PDGHelpers::isAJet(V1->getDaughter(0)->id)){
      sprintf((plabel_.plabel)[2], "qj");
      sprintf((plabel_.plabel)[3], "qj");
      zcouple_.l1 *= sqrt(6.);
      nqcdjets_.nqcdjets += 2;
    }

    if (PDGHelpers::isANeutrino(V2->getDaughter(1)->id)){
      sprintf((plabel_.plabel)[4], "el");
      sprintf((plabel_.plabel)[5], "na");
    }
    else if (PDGHelpers::isAJet(V2->getDaughter(1)->id)){
      sprintf((plabel_.plabel)[4], "qj");
      sprintf((plabel_.plabel)[5], "qj");
      zcouple_.l1 *= sqrt(6.);
      nqcdjets_.nqcdjets += 2;
    }
    if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(1)->id)) return false; // MCFM does not support WW->4q

  }
  else if (
    ndau>=4
    &&
    ((production == TVar::ZZINDEPENDENT || production == TVar::ZZQQB) && process == TVar::bkgZJJ)
    ){
    // -- 44 '  f(p1)+f(p2) --> Z^0(-->e^-(p3)+e^+(p4))+f(p5)+f(p6)'
    // these settings are identical to use the chooser_() function

    bveg1_mcfm_.ndim=10;
    breit_.n2=0;
    breit_.n3=1;
    nqcdjets_.nqcdjets=2;
    sprintf((plabel_.plabel)[2], "el");
    sprintf((plabel_.plabel)[3], "ea");
    sprintf((plabel_.plabel)[4], "pp");
    sprintf((plabel_.plabel)[5], "pp");
    sprintf((plabel_.plabel)[6], "pp");

    if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
      zcouple_.q1=-1.0;
      zcouple_.l1=zcouple_.le;
      zcouple_.r1=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
      zcouple_.q1=0;
      zcouple_.l1=zcouple_.ln;
      zcouple_.r1=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q1=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l1=zcouple_.l[jetid];
        zcouple_.r1=zcouple_.r[jetid];
      }
      else{
        zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    nwz_.nwz=0;
    breit_.mass3=masses_mcfm_.zmass;
    breit_.width3=masses_mcfm_.zwidth;

  }
  else if (
    ndau==3
    &&
    (production == TVar::ZZQQB && process == TVar::bkgZGamma)
    ){
    // -- 300 '  f(p1)+f(p2) --> Z^0(-->e^-(p3)+e^+(p4))+gamma(p5)'
    // -- 305 '  f(p1)+f(p2) --> Z^0(-->3*(nu(p3)+nu~(p4)))-(sum over 3 nu)+gamma(p5)'

    nqcdjets_.nqcdjets=0;
    bveg1_mcfm_.ndim=7;
    breit_.n2=0;
    breit_.n3=1;
    breit_.mass3=masses_mcfm_.zmass;
    breit_.width3=masses_mcfm_.zwidth;
    nwz_.nwz=0;
    sprintf((plabel_.plabel)[4], "ga");
    sprintf((plabel_.plabel)[5], "pp");
    lastphot_.lastphot=5;

    if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
      // -- 300 '  f(p1)+f(p2) --> Z^0(-->e^-(p3)+e^+(p4))+gamma(p5)'
      sprintf((plabel_.plabel)[2], "el");
      sprintf((plabel_.plabel)[3], "ea");
      zcouple_.q1=-1.0;
      zcouple_.l1=zcouple_.le;
      zcouple_.r1=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
      // -- 305 '  f(p1)+f(p2) --> Z^0(-->3*(nu(p3)+nu~(p4)))-(sum over 3 nu)+gamma(p5)'
      sprintf((plabel_.plabel)[2], "nl");
      sprintf((plabel_.plabel)[3], "na");
      zcouple_.q1=0;
      zcouple_.l1=zcouple_.ln;
      zcouple_.r1=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
      sprintf((plabel_.plabel)[2], "el");// Trick MCFM, not implemented in MCFM properly
      sprintf((plabel_.plabel)[3], "ea");
      nqcdjets_.nqcdjets += 2;
      zcouple_.q1=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l1=zcouple_.l[jetid];
        zcouple_.r1=zcouple_.r[jetid];
      }
      else{
        zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

  }
  else if (
    ndau>=4
    &&
    production == TVar::ZZGG
    &&
    (isZZ && (process==TVar::bkgZZ || process==TVar::HSMHiggs || process == TVar::bkgZZ_SMHiggs))
    ){
    // 114 '  f(p1)+f(p2) --> H(--> Z^0(mu^-(p3)+mu^+(p4)) + Z^0(e^-(p5)+e^+(p6))' 'N'
    /*
    nprocs:
    c--- 128 '  f(p1)+f(p2) --> H(--> Z^0(e^-(p3)+e^+(p4)) + Z^0(mu^-(p5)+mu^+(p6)) [top, bottom loops, exact]' 'L'
    c--- 129 '  f(p1)+f(p2) --> H(--> Z^0(e^-(p3)+e^+(p4)) + Z^0(mu^-(p5)+mu^+(p6)) [only H, gg->ZZ intf.]' 'L' -> NOT IMPLEMENTED
    c--- 130 '  f(p1)+f(p2) --> H(--> Z^0(e^-(p3)+e^+(p4)) + Z^0(mu^-(p5)+mu^+(p6)) [H squared and H, gg->ZZ intf.]' 'L'
    c--- 131 '  f(p1)+f(p2) --> Z^0(e^-(p3)+e^+(p4)) + Z^0(mu^-(p5)+mu^+(p6) [gg only, (H + gg->ZZ) squared]' 'L'
    c--- 132 '  f(p1)+f(p2) --> Z^0(e^-(p3)+e^+(p4)) + Z^0(mu^-(p5)+mu^+(p6) [(gg->ZZ) squared]' 'L'
    */

    npart_.npart=4;
    nqcdjets_.nqcdjets=0;

    bveg1_mcfm_.ndim=10;

    breit_.n2=1;
    breit_.n3=1;

    breit_.mass2 =masses_mcfm_.zmass;
    breit_.width2=masses_mcfm_.zwidth;
    breit_.mass3 =masses_mcfm_.zmass;
    breit_.width3=masses_mcfm_.zwidth;

    if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
      zcouple_.q1=-1.0;
      zcouple_.l1=zcouple_.le;
      zcouple_.r1=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
      zcouple_.q1=0;
      zcouple_.l1=zcouple_.ln;
      zcouple_.r1=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q1=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l1=zcouple_.l[jetid];
        zcouple_.r1=zcouple_.r[jetid];
      }
      else{
        zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    if (PDGHelpers::isALepton(V2->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(1)->id)){
      zcouple_.q2=-1.0;
      zcouple_.l2=zcouple_.le;
      zcouple_.r2=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V2->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(1)->id)){
      zcouple_.q2=0;
      zcouple_.l2=zcouple_.ln;
      zcouple_.r2=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V2->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q2=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V2->getDaughter(0)->id) ? abs(V2->getDaughter(1)->id) : abs(V2->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l2=zcouple_.l[jetid];
        zcouple_.r2=zcouple_.r[jetid];
      }
      else{
        zcouple_.l2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    vsymfact_.vsymfact=1.0;
    interference_.interference=false;
    if (definiteInterf && (leptonInterf==TVar::DefaultLeptonInterf || leptonInterf==TVar::InterfOn)){
      vsymfact_.vsymfact=0.5;
      interference_.interference=true;
    }

  }
  else if (
    ndau>=4
    &&
    production == TVar::ZZGG
    &&
    (
    (
    (isZZ && (process==TVar::bkgWWZZ || process==TVar::HSMHiggs_ZZWW || process == TVar::bkgWWZZ_SMHiggs))
    )
    ||
    (
    (isWW && (process==TVar::bkgWW || process==TVar::HSMHiggs || process == TVar::bkgWW_SMHiggs))
    )
    ||
    (
    (isWW && (process==TVar::bkgWWZZ || process==TVar::HSMHiggs_ZZWW || process == TVar::bkgWWZZ_SMHiggs))
    )
    )
    ){ // gg->VV
    // Processes 1281, 1311, 1321
    if (
      PDGHelpers::isAJet(V1->getDaughter(0)->id)
      ||
      PDGHelpers::isAJet(V1->getDaughter(1)->id)
      ||
      PDGHelpers::isAJet(V2->getDaughter(0)->id)
      ||
      PDGHelpers::isAJet(V2->getDaughter(1)->id)
      ) return false; // W->qq is not supported.

    sprintf((plabel_.plabel)[2], "el");
    sprintf((plabel_.plabel)[3], "ea");
    sprintf((plabel_.plabel)[4], "nl");
    sprintf((plabel_.plabel)[5], "na");
    sprintf((plabel_.plabel)[6], "pp");

    nwz_.nwz=0;
    nqcdjets_.nqcdjets=0;
    bveg1_mcfm_.ndim=10;
    breit_.n2=1;
    breit_.n3=1;
    nuflav_.nuflav=1; // Keep this at 1. Mela controls how many flavors in a more exact way.

    if (isWW){
      if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(1)->id)){
        zcouple_.q1=-1.0;
        zcouple_.l1=zcouple_.le;
        zcouple_.r1=zcouple_.re;
      }
      else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(1)->id)){
        zcouple_.q1=0;
        zcouple_.l1=zcouple_.ln;
        zcouple_.r1=zcouple_.rn;
      }
      else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(1)->id)){
        nqcdjets_.nqcdjets += 2;
        zcouple_.q1=-1.0;
        int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V2->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
        if (jetid>0 && jetid<6){
          zcouple_.l1=zcouple_.l[jetid];
          zcouple_.r1=zcouple_.r[jetid];
        }
        else{
          zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
          zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        }
      }
      else return false;

      if (PDGHelpers::isALepton(V2->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
        zcouple_.q2=-1.0;
        zcouple_.l2=zcouple_.le;
        zcouple_.r2=zcouple_.re;
      }
      else if (PDGHelpers::isANeutrino(V2->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
        zcouple_.q2=0;
        zcouple_.l2=zcouple_.ln;
        zcouple_.r2=zcouple_.rn;
      }
      else if (PDGHelpers::isAJet(V2->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
        nqcdjets_.nqcdjets += 2;
        zcouple_.q2=-1.0;
        int jetid=(PDGHelpers::isAnUnknownJet(V2->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V2->getDaughter(0)->id));
        if (jetid>0 && jetid<6){
          zcouple_.l2=zcouple_.l[jetid];
          zcouple_.r2=zcouple_.r[jetid];
        }
        else{
          zcouple_.l2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
          zcouple_.r2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        }
      }
      else return false;
    }
    else if (isZZ){
      if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
        zcouple_.q1=-1.0;
        zcouple_.l1=zcouple_.le;
        zcouple_.r1=zcouple_.re;
      }
      else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
        zcouple_.q1=0;
        zcouple_.l1=zcouple_.ln;
        zcouple_.r1=zcouple_.rn;
      }
      else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
        nqcdjets_.nqcdjets += 2;
        zcouple_.q1=-1.0;
        int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
        if (jetid>0 && jetid<6){
          zcouple_.l1=zcouple_.l[jetid];
          zcouple_.r1=zcouple_.r[jetid];
        }
        else{
          zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
          zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        }
      }
      else return false;

      if (PDGHelpers::isALepton(V2->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(1)->id)){
        zcouple_.q2=-1.0;
        zcouple_.l2=zcouple_.le;
        zcouple_.r2=zcouple_.re;
      }
      else if (PDGHelpers::isANeutrino(V2->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(1)->id)){
        zcouple_.q2=0;
        zcouple_.l2=zcouple_.ln;
        zcouple_.r2=zcouple_.rn;
      }
      else if (PDGHelpers::isAJet(V2->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(1)->id)){
        nqcdjets_.nqcdjets += 2;
        zcouple_.q2=-1.0;
        int jetid=(PDGHelpers::isAnUnknownJet(V2->getDaughter(0)->id) ? abs(V2->getDaughter(1)->id) : abs(V2->getDaughter(0)->id));
        if (jetid>0 && jetid<6){
          zcouple_.l2=zcouple_.l[jetid];
          zcouple_.r2=zcouple_.r[jetid];
        }
        else{
          zcouple_.l2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
          zcouple_.r2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        }
      }
      else return false;
    }

    if(
      (
      isWW
      &&
      (
      (
      !PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id)
      &&
      !PDGHelpers::isAnUnknownJet(V2->getDaughter(1)->id)
      &&
      V1->getDaughter(0)->id!=-V2->getDaughter(1)->id
      ) // WW->u?+?cb is WW-only
      ||
      (
      !PDGHelpers::isAnUnknownJet(V1->getDaughter(1)->id)
      &&
      !PDGHelpers::isAnUnknownJet(V2->getDaughter(0)->id)
      &&
      V1->getDaughter(1)->id!=-V2->getDaughter(0)->id
      ) // WW->?db+s? is WW-only
      )
      )
      ||
      (
      (isWW && (process==TVar::bkgWW || process==TVar::HSMHiggs || process == TVar::bkgWW_SMHiggs))
      )
      ) sprintf(runstring_.runstring, "test_ww");

    if (
      isZZ
      &&
      (
      (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(0)->id)) // llqq
      ||
      (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(0)->id)) // nnqq
      ||
      (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(0)->id) && (abs(V1->getDaughter(0)->id)+1)!=abs(V2->getDaughter(0)->id)) // eg. ee nu_mu nu_mu 
      || // Check wrong configurations anyway
      (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(0)->id)) // qqll
      ||
      (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(0)->id)) // qqnn
      ||
      (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(0)->id) && abs(V1->getDaughter(0)->id)!=(abs(V2->getDaughter(0)->id)+1)) // eg. nu_mu nu_mu ee
      )
      ) sprintf(runstring_.runstring, "test_zz");

  }
  // JJ + VV->4f
  else if ( // Check for support in qq'H+2J
    ndau>=4
    &&
    production == TVar::JJVBF
    &&
    (isZZ && (process==TVar::bkgZZ || process==TVar::HSMHiggs || process == TVar::bkgZZ_SMHiggs))
    ){
    // 220 '  f(p1)+f(p2) --> Z(e-(p3),e^+(p4))Z(mu-(p5),mu+(p6)))+f(p7)+f(p8) [weak]' 'L'

    if (process==TVar::bkgZZ){
      wwbits_.Hbit[0]=0;
      wwbits_.Hbit[1]=0;
      wwbits_.Bbit[0]=1;
      wwbits_.Bbit[1]=0;
    }
    else if (process==TVar::bkgZZ_SMHiggs){
      wwbits_.Hbit[0]=1;
      wwbits_.Hbit[1]=0;
      wwbits_.Bbit[0]=1;
      wwbits_.Bbit[1]=0;
    }
    else{
      wwbits_.Hbit[0]=1;
      wwbits_.Hbit[1]=0;
      wwbits_.Bbit[0]=0;
      wwbits_.Bbit[1]=0;
    }

    npart_.npart=6;
    nwz_.nwz=2;

    if (PDGHelpers::isALepton(V1->getDaughter(0)->id) && PDGHelpers::isALepton(V1->getDaughter(1)->id)){
      zcouple_.q1=-1.0;
      zcouple_.l1=zcouple_.le;
      zcouple_.r1=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V1->getDaughter(0)->id) && PDGHelpers::isANeutrino(V1->getDaughter(1)->id)){
      zcouple_.q1=0;
      zcouple_.l1=zcouple_.ln;
      zcouple_.r1=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V1->getDaughter(0)->id) && PDGHelpers::isAJet(V1->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q1=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V1->getDaughter(0)->id) ? abs(V1->getDaughter(1)->id) : abs(V1->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l1=zcouple_.l[jetid];
        zcouple_.r1=zcouple_.r[jetid];
      }
      else{
        zcouple_.l1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r1=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    if (PDGHelpers::isALepton(V2->getDaughter(0)->id) && PDGHelpers::isALepton(V2->getDaughter(1)->id)){
      zcouple_.q2=-1.0;
      zcouple_.l2=zcouple_.le;
      zcouple_.r2=zcouple_.re;
    }
    else if (PDGHelpers::isANeutrino(V2->getDaughter(0)->id) && PDGHelpers::isANeutrino(V2->getDaughter(1)->id)){
      zcouple_.q2=0;
      zcouple_.l2=zcouple_.ln;
      zcouple_.r2=zcouple_.rn;
    }
    else if (PDGHelpers::isAJet(V2->getDaughter(0)->id) && PDGHelpers::isAJet(V2->getDaughter(1)->id)){
      nqcdjets_.nqcdjets += 2;
      zcouple_.q2=-1.0;
      int jetid=(PDGHelpers::isAnUnknownJet(V2->getDaughter(0)->id) ? abs(V2->getDaughter(1)->id) : abs(V2->getDaughter(0)->id));
      if (jetid>0 && jetid<6){
        zcouple_.l2=zcouple_.l[jetid];
        zcouple_.r2=zcouple_.r[jetid];
      }
      else{
        zcouple_.l2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
        zcouple_.r2=sqrt(((pow(zcouple_.l[2], 2)+pow(zcouple_.r[2], 2))*3.+(pow(zcouple_.l[1], 2)+pow(zcouple_.r[1], 2))*2.)/10.);
      }
    }
    else return false;

    bveg1_mcfm_.ndim=16;
    nqcdjets_.nqcdjets=2;

    breit_.n2=1;
    breit_.n3=1;

    breit_.mass2 =masses_mcfm_.zmass;
    breit_.width2=masses_mcfm_.zwidth;
    breit_.mass3 =masses_mcfm_.zmass;
    breit_.width3=masses_mcfm_.zwidth;

    vsymfact_.vsymfact=1.0;
    interference_.interference=false;
    if (definiteInterf && (leptonInterf==TVar::DefaultLeptonInterf || leptonInterf==TVar::InterfOn)){
      vsymfact_.vsymfact=0.5;
      interference_.interference=true;
    }

  }
  else{
    cerr <<"TUtil::MCFM_chooser: Can't identify Process: " << process << endl;
    cerr <<"TUtil::MCFM_chooser: ndau: " << ndau << '\t';
    cerr <<"TUtil::MCFM_chooser: isZZ: " << isZZ << '\t';
    cerr <<"TUtil::MCFM_chooser: isWW: " << isWW << '\t';
    cerr << endl;
    return false;
  }
  return true;
}

void TUtil::InitJHUGenMELA(const char* pathtoPDFSet, int PDFMember){
  char path_pdf_c[200];
  sprintf(path_pdf_c, "%s", pathtoPDFSet);
  int pathpdfLength = strlen(path_pdf_c);
  __modjhugen_MOD_initfirsttime(path_pdf_c, &pathpdfLength, &PDFMember);
}
void TUtil::SetJHUGenHiggsMassWidth(double MReso, double GaReso){
  const double GeV = 1./100.;
  MReso *= GeV; // GeV units in JHUGen
  GaReso *= GeV; // GeV units in JHUGen
  __modjhugenmela_MOD_sethiggsmasswidth(&MReso, &GaReso);
}
void TUtil::SetJHUGenDistinguishWWCouplings(bool doAllow){
  int iAllow = (doAllow ? 1 : 0);
  __modjhugenmela_MOD_setdistinguishwwcouplingsflag(&iAllow);
}
void TUtil::SetMCFMSpinZeroVVCouplings(bool useBSM, SpinZeroCouplings* Hcouplings, bool forceZZ){
  if (!useBSM){
    spinzerohiggs_anomcoupl_.AllowAnomalousCouplings = false;
    spinzerohiggs_anomcoupl_.distinguish_HWWcouplings = false;

    /***** REGULAR RESONANCE *****/
    //
    spinzerohiggs_anomcoupl_.cz_q1sq = 0;
    spinzerohiggs_anomcoupl_.Lambda_z11 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z21 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z31 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z41 = 100;
    spinzerohiggs_anomcoupl_.cz_q2sq = 0;
    spinzerohiggs_anomcoupl_.Lambda_z12 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z22 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z32 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z42 = 100;
    spinzerohiggs_anomcoupl_.cz_q12sq = 0;
    spinzerohiggs_anomcoupl_.Lambda_z10 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z20 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z30 = 100;
    spinzerohiggs_anomcoupl_.Lambda_z40 = 100;
    //
    spinzerohiggs_anomcoupl_.cw_q1sq = 0;
    spinzerohiggs_anomcoupl_.Lambda_w11 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w21 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w31 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w41 = 100;
    spinzerohiggs_anomcoupl_.cw_q2sq = 0;
    spinzerohiggs_anomcoupl_.Lambda_w12 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w22 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w32 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w42 = 100;
    spinzerohiggs_anomcoupl_.cw_q12sq = 0;
    spinzerohiggs_anomcoupl_.Lambda_w10 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w20 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w30 = 100;
    spinzerohiggs_anomcoupl_.Lambda_w40 = 100;
    //
    spinzerohiggs_anomcoupl_.ghz1[0] = 1; spinzerohiggs_anomcoupl_.ghz1[1] = 0;
    spinzerohiggs_anomcoupl_.ghw1[0] = 1; spinzerohiggs_anomcoupl_.ghw1[1] = 0;
    for (int im=0; im<2; im++){
      spinzerohiggs_anomcoupl_.ghz2[im] =  0;
      spinzerohiggs_anomcoupl_.ghz3[im] =  0;
      spinzerohiggs_anomcoupl_.ghz4[im] =  0;
      spinzerohiggs_anomcoupl_.ghz1_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghz1_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghz1_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghz1_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghz1_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghz1_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghz1_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghz2_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghz3_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghz4_prime7[im] = 0;
      //
      spinzerohiggs_anomcoupl_.ghzgs1_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghzgs2[im] = 0;
      spinzerohiggs_anomcoupl_.ghzgs3[im] = 0;
      spinzerohiggs_anomcoupl_.ghzgs4[im] = 0;
      spinzerohiggs_anomcoupl_.ghgsgs2[im] = 0;
      spinzerohiggs_anomcoupl_.ghgsgs3[im] = 0;
      spinzerohiggs_anomcoupl_.ghgsgs4[im] = 0;
      //
      spinzerohiggs_anomcoupl_.ghw2[im] =  0;
      spinzerohiggs_anomcoupl_.ghw3[im] =  0;
      spinzerohiggs_anomcoupl_.ghw4[im] =  0;
      spinzerohiggs_anomcoupl_.ghw1_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghw1_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghw1_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghw1_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghw1_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghw1_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghw1_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghw2_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghw3_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.ghw4_prime7[im] = 0;
    }
    /***** END REGULAR RESONANCE *****/
    //
    /***** SECOND RESONANCE *****/
    //
    spinzerohiggs_anomcoupl_.c2z_q1sq = 0;
    spinzerohiggs_anomcoupl_.Lambda2_z11 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z21 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z31 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z41 = 100;
    spinzerohiggs_anomcoupl_.c2z_q2sq = 0;
    spinzerohiggs_anomcoupl_.Lambda2_z12 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z22 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z32 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z42 = 100;
    spinzerohiggs_anomcoupl_.c2z_q12sq = 0;
    spinzerohiggs_anomcoupl_.Lambda2_z10 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z20 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z30 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_z40 = 100;
    //
    spinzerohiggs_anomcoupl_.c2w_q1sq = 0;
    spinzerohiggs_anomcoupl_.Lambda2_w11 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w21 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w31 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w41 = 100;
    spinzerohiggs_anomcoupl_.c2w_q2sq = 0;
    spinzerohiggs_anomcoupl_.Lambda2_w12 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w22 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w32 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w42 = 100;
    spinzerohiggs_anomcoupl_.c2w_q12sq = 0;
    spinzerohiggs_anomcoupl_.Lambda2_w10 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w20 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w30 = 100;
    spinzerohiggs_anomcoupl_.Lambda2_w40 = 100;
    //
    spinzerohiggs_anomcoupl_.gh2z1[0] = 0; spinzerohiggs_anomcoupl_.gh2z1[1] = 0;
    spinzerohiggs_anomcoupl_.gh2w1[0] = 0; spinzerohiggs_anomcoupl_.gh2w1[1] = 0;
    for (int im=0; im<2; im++){
      spinzerohiggs_anomcoupl_.gh2z2[im] =  0;
      spinzerohiggs_anomcoupl_.gh2z3[im] =  0;
      spinzerohiggs_anomcoupl_.gh2z4[im] =  0;
      spinzerohiggs_anomcoupl_.gh2z1_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z1_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z1_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z1_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z1_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z1_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z1_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z2_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z3_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2z4_prime7[im] = 0;
      //
      spinzerohiggs_anomcoupl_.gh2zgs1_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2zgs2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2zgs3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2zgs4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2gsgs2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2gsgs3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2gsgs4[im] = 0;
      //
      spinzerohiggs_anomcoupl_.gh2w2[im] =  0;
      spinzerohiggs_anomcoupl_.gh2w3[im] =  0;
      spinzerohiggs_anomcoupl_.gh2w4[im] =  0;
      spinzerohiggs_anomcoupl_.gh2w1_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w1_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w1_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w1_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w1_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w1_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w1_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w2_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w3_prime7[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime2[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime3[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime4[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime5[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime6[im] = 0;
      spinzerohiggs_anomcoupl_.gh2w4_prime7[im] = 0;
    }
    /***** END SECOND RESONANCE *****/
  }
  else{
    spinzerohiggs_anomcoupl_.AllowAnomalousCouplings = true;
    spinzerohiggs_anomcoupl_.distinguish_HWWcouplings = (Hcouplings->separateWWZZcouplings && !forceZZ);

    /***** REGULAR RESONANCE *****/
    //
    spinzerohiggs_anomcoupl_.cz_q1sq = (Hcouplings->HzzCLambda_qsq)[0];
    spinzerohiggs_anomcoupl_.Lambda_z11 = (Hcouplings->HzzLambda_qsq)[0][0];
    spinzerohiggs_anomcoupl_.Lambda_z21 = (Hcouplings->HzzLambda_qsq)[1][0];
    spinzerohiggs_anomcoupl_.Lambda_z31 = (Hcouplings->HzzLambda_qsq)[2][0];
    spinzerohiggs_anomcoupl_.Lambda_z41 = (Hcouplings->HzzLambda_qsq)[3][0];
    spinzerohiggs_anomcoupl_.cz_q2sq = (Hcouplings->HzzCLambda_qsq)[1];
    spinzerohiggs_anomcoupl_.Lambda_z12 = (Hcouplings->HzzLambda_qsq)[0][1];
    spinzerohiggs_anomcoupl_.Lambda_z22 = (Hcouplings->HzzLambda_qsq)[1][1];
    spinzerohiggs_anomcoupl_.Lambda_z32 = (Hcouplings->HzzLambda_qsq)[2][1];
    spinzerohiggs_anomcoupl_.Lambda_z42 = (Hcouplings->HzzLambda_qsq)[3][1];
    spinzerohiggs_anomcoupl_.cz_q12sq = (Hcouplings->HzzCLambda_qsq)[2];
    spinzerohiggs_anomcoupl_.Lambda_z10 = (Hcouplings->HzzLambda_qsq)[0][2];
    spinzerohiggs_anomcoupl_.Lambda_z20 = (Hcouplings->HzzLambda_qsq)[1][2];
    spinzerohiggs_anomcoupl_.Lambda_z30 = (Hcouplings->HzzLambda_qsq)[2][2];
    spinzerohiggs_anomcoupl_.Lambda_z40 = (Hcouplings->HzzLambda_qsq)[3][2];
    //
    for (int im=0; im<2; im++){
      spinzerohiggs_anomcoupl_.ghz1[im] = (Hcouplings->Hzzcoupl)[0][im];
      spinzerohiggs_anomcoupl_.ghz2[im] = (Hcouplings->Hzzcoupl)[1][im];
      spinzerohiggs_anomcoupl_.ghz3[im] = (Hcouplings->Hzzcoupl)[2][im];
      spinzerohiggs_anomcoupl_.ghz4[im] = (Hcouplings->Hzzcoupl)[3][im];
      spinzerohiggs_anomcoupl_.ghz1_prime[im] = (Hcouplings->Hzzcoupl)[10][im];
      spinzerohiggs_anomcoupl_.ghz1_prime2[im] = (Hcouplings->Hzzcoupl)[11][im];
      spinzerohiggs_anomcoupl_.ghz1_prime3[im] = (Hcouplings->Hzzcoupl)[12][im];
      spinzerohiggs_anomcoupl_.ghz1_prime4[im] = (Hcouplings->Hzzcoupl)[13][im];
      spinzerohiggs_anomcoupl_.ghz1_prime5[im] = (Hcouplings->Hzzcoupl)[14][im];
      spinzerohiggs_anomcoupl_.ghz2_prime[im] = (Hcouplings->Hzzcoupl)[15][im];
      spinzerohiggs_anomcoupl_.ghz2_prime2[im] = (Hcouplings->Hzzcoupl)[16][im];
      spinzerohiggs_anomcoupl_.ghz2_prime3[im] = (Hcouplings->Hzzcoupl)[17][im];
      spinzerohiggs_anomcoupl_.ghz2_prime4[im] = (Hcouplings->Hzzcoupl)[18][im];
      spinzerohiggs_anomcoupl_.ghz2_prime5[im] = (Hcouplings->Hzzcoupl)[19][im];
      spinzerohiggs_anomcoupl_.ghz3_prime[im] = (Hcouplings->Hzzcoupl)[20][im];
      spinzerohiggs_anomcoupl_.ghz3_prime2[im] = (Hcouplings->Hzzcoupl)[21][im];
      spinzerohiggs_anomcoupl_.ghz3_prime3[im] = (Hcouplings->Hzzcoupl)[22][im];
      spinzerohiggs_anomcoupl_.ghz3_prime4[im] = (Hcouplings->Hzzcoupl)[23][im];
      spinzerohiggs_anomcoupl_.ghz3_prime5[im] = (Hcouplings->Hzzcoupl)[24][im];
      spinzerohiggs_anomcoupl_.ghz4_prime[im] = (Hcouplings->Hzzcoupl)[25][im];
      spinzerohiggs_anomcoupl_.ghz4_prime2[im] = (Hcouplings->Hzzcoupl)[26][im];
      spinzerohiggs_anomcoupl_.ghz4_prime3[im] = (Hcouplings->Hzzcoupl)[27][im];
      spinzerohiggs_anomcoupl_.ghz4_prime4[im] = (Hcouplings->Hzzcoupl)[28][im];
      spinzerohiggs_anomcoupl_.ghz4_prime5[im] = (Hcouplings->Hzzcoupl)[29][im];
      spinzerohiggs_anomcoupl_.ghz1_prime6[im] = (Hcouplings->Hzzcoupl)[31][im];
      spinzerohiggs_anomcoupl_.ghz1_prime7[im] = (Hcouplings->Hzzcoupl)[32][im];
      spinzerohiggs_anomcoupl_.ghz2_prime6[im] = (Hcouplings->Hzzcoupl)[33][im];
      spinzerohiggs_anomcoupl_.ghz2_prime7[im] = (Hcouplings->Hzzcoupl)[34][im];
      spinzerohiggs_anomcoupl_.ghz3_prime6[im] = (Hcouplings->Hzzcoupl)[35][im];
      spinzerohiggs_anomcoupl_.ghz3_prime7[im] = (Hcouplings->Hzzcoupl)[36][im];
      spinzerohiggs_anomcoupl_.ghz4_prime6[im] = (Hcouplings->Hzzcoupl)[37][im];
      spinzerohiggs_anomcoupl_.ghz4_prime7[im] = (Hcouplings->Hzzcoupl)[38][im];
      //
      spinzerohiggs_anomcoupl_.ghzgs1_prime2[im] = (Hcouplings->Hzzcoupl)[30][im];
      spinzerohiggs_anomcoupl_.ghzgs2[im] = (Hcouplings->Hzzcoupl)[4][im];
      spinzerohiggs_anomcoupl_.ghzgs3[im] = (Hcouplings->Hzzcoupl)[5][im];
      spinzerohiggs_anomcoupl_.ghzgs4[im] = (Hcouplings->Hzzcoupl)[6][im];
      spinzerohiggs_anomcoupl_.ghgsgs2[im] = (Hcouplings->Hzzcoupl)[7][im];
      spinzerohiggs_anomcoupl_.ghgsgs3[im] = (Hcouplings->Hzzcoupl)[8][im];
      spinzerohiggs_anomcoupl_.ghgsgs4[im] = (Hcouplings->Hzzcoupl)[9][im];
    }
    //
    if (spinzerohiggs_anomcoupl_.distinguish_HWWcouplings){
      //
      spinzerohiggs_anomcoupl_.cw_q1sq = (Hcouplings->HwwCLambda_qsq)[0];
      spinzerohiggs_anomcoupl_.Lambda_w11 = (Hcouplings->HwwLambda_qsq)[0][0];
      spinzerohiggs_anomcoupl_.Lambda_w21 = (Hcouplings->HwwLambda_qsq)[1][0];
      spinzerohiggs_anomcoupl_.Lambda_w31 = (Hcouplings->HwwLambda_qsq)[2][0];
      spinzerohiggs_anomcoupl_.Lambda_w41 = (Hcouplings->HwwLambda_qsq)[3][0];
      spinzerohiggs_anomcoupl_.cw_q2sq = (Hcouplings->HwwCLambda_qsq)[1];
      spinzerohiggs_anomcoupl_.Lambda_w12 = (Hcouplings->HwwLambda_qsq)[0][1];
      spinzerohiggs_anomcoupl_.Lambda_w22 = (Hcouplings->HwwLambda_qsq)[1][1];
      spinzerohiggs_anomcoupl_.Lambda_w32 = (Hcouplings->HwwLambda_qsq)[2][1];
      spinzerohiggs_anomcoupl_.Lambda_w42 = (Hcouplings->HwwLambda_qsq)[3][1];
      spinzerohiggs_anomcoupl_.cw_q12sq = (Hcouplings->HwwCLambda_qsq)[2];
      spinzerohiggs_anomcoupl_.Lambda_w10 = (Hcouplings->HwwLambda_qsq)[0][2];
      spinzerohiggs_anomcoupl_.Lambda_w20 = (Hcouplings->HwwLambda_qsq)[1][2];
      spinzerohiggs_anomcoupl_.Lambda_w30 = (Hcouplings->HwwLambda_qsq)[2][2];
      spinzerohiggs_anomcoupl_.Lambda_w40 = (Hcouplings->HwwLambda_qsq)[3][2];
      //
      for (int im=0; im<2; im++){
        spinzerohiggs_anomcoupl_.ghw1[im] = (Hcouplings->Hwwcoupl)[0][im];
        spinzerohiggs_anomcoupl_.ghw2[im] = (Hcouplings->Hwwcoupl)[1][im];
        spinzerohiggs_anomcoupl_.ghw3[im] = (Hcouplings->Hwwcoupl)[2][im];
        spinzerohiggs_anomcoupl_.ghw4[im] = (Hcouplings->Hwwcoupl)[3][im];
        spinzerohiggs_anomcoupl_.ghw1_prime[im] = (Hcouplings->Hwwcoupl)[10][im];
        spinzerohiggs_anomcoupl_.ghw1_prime2[im] = (Hcouplings->Hwwcoupl)[11][im];
        spinzerohiggs_anomcoupl_.ghw1_prime3[im] = (Hcouplings->Hwwcoupl)[12][im];
        spinzerohiggs_anomcoupl_.ghw1_prime4[im] = (Hcouplings->Hwwcoupl)[13][im];
        spinzerohiggs_anomcoupl_.ghw1_prime5[im] = (Hcouplings->Hwwcoupl)[14][im];
        spinzerohiggs_anomcoupl_.ghw2_prime[im] = (Hcouplings->Hwwcoupl)[15][im];
        spinzerohiggs_anomcoupl_.ghw2_prime2[im] = (Hcouplings->Hwwcoupl)[16][im];
        spinzerohiggs_anomcoupl_.ghw2_prime3[im] = (Hcouplings->Hwwcoupl)[17][im];
        spinzerohiggs_anomcoupl_.ghw2_prime4[im] = (Hcouplings->Hwwcoupl)[18][im];
        spinzerohiggs_anomcoupl_.ghw2_prime5[im] = (Hcouplings->Hwwcoupl)[19][im];
        spinzerohiggs_anomcoupl_.ghw3_prime[im] = (Hcouplings->Hwwcoupl)[20][im];
        spinzerohiggs_anomcoupl_.ghw3_prime2[im] = (Hcouplings->Hwwcoupl)[21][im];
        spinzerohiggs_anomcoupl_.ghw3_prime3[im] = (Hcouplings->Hwwcoupl)[22][im];
        spinzerohiggs_anomcoupl_.ghw3_prime4[im] = (Hcouplings->Hwwcoupl)[23][im];
        spinzerohiggs_anomcoupl_.ghw3_prime5[im] = (Hcouplings->Hwwcoupl)[24][im];
        spinzerohiggs_anomcoupl_.ghw4_prime[im] = (Hcouplings->Hwwcoupl)[25][im];
        spinzerohiggs_anomcoupl_.ghw4_prime2[im] = (Hcouplings->Hwwcoupl)[26][im];
        spinzerohiggs_anomcoupl_.ghw4_prime3[im] = (Hcouplings->Hwwcoupl)[27][im];
        spinzerohiggs_anomcoupl_.ghw4_prime4[im] = (Hcouplings->Hwwcoupl)[28][im];
        spinzerohiggs_anomcoupl_.ghw4_prime5[im] = (Hcouplings->Hwwcoupl)[29][im];
        spinzerohiggs_anomcoupl_.ghw1_prime6[im] = (Hcouplings->Hwwcoupl)[31][im];
        spinzerohiggs_anomcoupl_.ghw1_prime7[im] = (Hcouplings->Hwwcoupl)[32][im];
        spinzerohiggs_anomcoupl_.ghw2_prime6[im] = (Hcouplings->Hwwcoupl)[33][im];
        spinzerohiggs_anomcoupl_.ghw2_prime7[im] = (Hcouplings->Hwwcoupl)[34][im];
        spinzerohiggs_anomcoupl_.ghw3_prime6[im] = (Hcouplings->Hwwcoupl)[35][im];
        spinzerohiggs_anomcoupl_.ghw3_prime7[im] = (Hcouplings->Hwwcoupl)[36][im];
        spinzerohiggs_anomcoupl_.ghw4_prime6[im] = (Hcouplings->Hwwcoupl)[37][im];
        spinzerohiggs_anomcoupl_.ghw4_prime7[im] = (Hcouplings->Hwwcoupl)[38][im];
      }
    }
    else{
      //
      spinzerohiggs_anomcoupl_.cw_q1sq = (Hcouplings->HzzCLambda_qsq)[0];
      spinzerohiggs_anomcoupl_.Lambda_w11 = (Hcouplings->HzzLambda_qsq)[0][0];
      spinzerohiggs_anomcoupl_.Lambda_w21 = (Hcouplings->HzzLambda_qsq)[1][0];
      spinzerohiggs_anomcoupl_.Lambda_w31 = (Hcouplings->HzzLambda_qsq)[2][0];
      spinzerohiggs_anomcoupl_.Lambda_w41 = (Hcouplings->HzzLambda_qsq)[3][0];
      spinzerohiggs_anomcoupl_.cw_q2sq = (Hcouplings->HzzCLambda_qsq)[1];
      spinzerohiggs_anomcoupl_.Lambda_w12 = (Hcouplings->HzzLambda_qsq)[0][1];
      spinzerohiggs_anomcoupl_.Lambda_w22 = (Hcouplings->HzzLambda_qsq)[1][1];
      spinzerohiggs_anomcoupl_.Lambda_w32 = (Hcouplings->HzzLambda_qsq)[2][1];
      spinzerohiggs_anomcoupl_.Lambda_w42 = (Hcouplings->HzzLambda_qsq)[3][1];
      spinzerohiggs_anomcoupl_.cw_q12sq = (Hcouplings->HzzCLambda_qsq)[2];
      spinzerohiggs_anomcoupl_.Lambda_w10 = (Hcouplings->HzzLambda_qsq)[0][2];
      spinzerohiggs_anomcoupl_.Lambda_w20 = (Hcouplings->HzzLambda_qsq)[1][2];
      spinzerohiggs_anomcoupl_.Lambda_w30 = (Hcouplings->HzzLambda_qsq)[2][2];
      spinzerohiggs_anomcoupl_.Lambda_w40 = (Hcouplings->HzzLambda_qsq)[3][2];
      //
      for (int im=0; im<2; im++){
        spinzerohiggs_anomcoupl_.ghw1[im] = (Hcouplings->Hzzcoupl)[0][im];
        spinzerohiggs_anomcoupl_.ghw2[im] = (Hcouplings->Hzzcoupl)[1][im];
        spinzerohiggs_anomcoupl_.ghw3[im] = (Hcouplings->Hzzcoupl)[2][im];
        spinzerohiggs_anomcoupl_.ghw4[im] = (Hcouplings->Hzzcoupl)[3][im];
        spinzerohiggs_anomcoupl_.ghw1_prime[im] = (Hcouplings->Hzzcoupl)[10][im];
        spinzerohiggs_anomcoupl_.ghw1_prime2[im] = (Hcouplings->Hzzcoupl)[11][im];
        spinzerohiggs_anomcoupl_.ghw1_prime3[im] = (Hcouplings->Hzzcoupl)[12][im];
        spinzerohiggs_anomcoupl_.ghw1_prime4[im] = (Hcouplings->Hzzcoupl)[13][im];
        spinzerohiggs_anomcoupl_.ghw1_prime5[im] = (Hcouplings->Hzzcoupl)[14][im];
        spinzerohiggs_anomcoupl_.ghw2_prime[im] = (Hcouplings->Hzzcoupl)[15][im];
        spinzerohiggs_anomcoupl_.ghw2_prime2[im] = (Hcouplings->Hzzcoupl)[16][im];
        spinzerohiggs_anomcoupl_.ghw2_prime3[im] = (Hcouplings->Hzzcoupl)[17][im];
        spinzerohiggs_anomcoupl_.ghw2_prime4[im] = (Hcouplings->Hzzcoupl)[18][im];
        spinzerohiggs_anomcoupl_.ghw2_prime5[im] = (Hcouplings->Hzzcoupl)[19][im];
        spinzerohiggs_anomcoupl_.ghw3_prime[im] = (Hcouplings->Hzzcoupl)[20][im];
        spinzerohiggs_anomcoupl_.ghw3_prime2[im] = (Hcouplings->Hzzcoupl)[21][im];
        spinzerohiggs_anomcoupl_.ghw3_prime3[im] = (Hcouplings->Hzzcoupl)[22][im];
        spinzerohiggs_anomcoupl_.ghw3_prime4[im] = (Hcouplings->Hzzcoupl)[23][im];
        spinzerohiggs_anomcoupl_.ghw3_prime5[im] = (Hcouplings->Hzzcoupl)[24][im];
        spinzerohiggs_anomcoupl_.ghw4_prime[im] = (Hcouplings->Hzzcoupl)[25][im];
        spinzerohiggs_anomcoupl_.ghw4_prime2[im] = (Hcouplings->Hzzcoupl)[26][im];
        spinzerohiggs_anomcoupl_.ghw4_prime3[im] = (Hcouplings->Hzzcoupl)[27][im];
        spinzerohiggs_anomcoupl_.ghw4_prime4[im] = (Hcouplings->Hzzcoupl)[28][im];
        spinzerohiggs_anomcoupl_.ghw4_prime5[im] = (Hcouplings->Hzzcoupl)[29][im];
        spinzerohiggs_anomcoupl_.ghw1_prime6[im] = (Hcouplings->Hzzcoupl)[31][im];
        spinzerohiggs_anomcoupl_.ghw1_prime7[im] = (Hcouplings->Hzzcoupl)[32][im];
        spinzerohiggs_anomcoupl_.ghw2_prime6[im] = (Hcouplings->Hzzcoupl)[33][im];
        spinzerohiggs_anomcoupl_.ghw2_prime7[im] = (Hcouplings->Hzzcoupl)[34][im];
        spinzerohiggs_anomcoupl_.ghw3_prime6[im] = (Hcouplings->Hzzcoupl)[35][im];
        spinzerohiggs_anomcoupl_.ghw3_prime7[im] = (Hcouplings->Hzzcoupl)[36][im];
        spinzerohiggs_anomcoupl_.ghw4_prime6[im] = (Hcouplings->Hzzcoupl)[37][im];
        spinzerohiggs_anomcoupl_.ghw4_prime7[im] = (Hcouplings->Hzzcoupl)[38][im];
      }
    }
    /***** END REGULAR RESONANCE *****/
    //
    /***** SECOND RESONANCE *****/
    //
    spinzerohiggs_anomcoupl_.c2z_q1sq = (Hcouplings->H2zzCLambda_qsq)[0];
    spinzerohiggs_anomcoupl_.Lambda2_z11 = (Hcouplings->H2zzLambda_qsq)[0][0];
    spinzerohiggs_anomcoupl_.Lambda2_z21 = (Hcouplings->H2zzLambda_qsq)[1][0];
    spinzerohiggs_anomcoupl_.Lambda2_z31 = (Hcouplings->H2zzLambda_qsq)[2][0];
    spinzerohiggs_anomcoupl_.Lambda2_z41 = (Hcouplings->H2zzLambda_qsq)[3][0];
    spinzerohiggs_anomcoupl_.c2z_q2sq = (Hcouplings->H2zzCLambda_qsq)[1];
    spinzerohiggs_anomcoupl_.Lambda2_z12 = (Hcouplings->H2zzLambda_qsq)[0][1];
    spinzerohiggs_anomcoupl_.Lambda2_z22 = (Hcouplings->H2zzLambda_qsq)[1][1];
    spinzerohiggs_anomcoupl_.Lambda2_z32 = (Hcouplings->H2zzLambda_qsq)[2][1];
    spinzerohiggs_anomcoupl_.Lambda2_z42 = (Hcouplings->H2zzLambda_qsq)[3][1];
    spinzerohiggs_anomcoupl_.c2z_q12sq = (Hcouplings->H2zzCLambda_qsq)[2];
    spinzerohiggs_anomcoupl_.Lambda2_z10 = (Hcouplings->H2zzLambda_qsq)[0][2];
    spinzerohiggs_anomcoupl_.Lambda2_z20 = (Hcouplings->H2zzLambda_qsq)[1][2];
    spinzerohiggs_anomcoupl_.Lambda2_z30 = (Hcouplings->H2zzLambda_qsq)[2][2];
    spinzerohiggs_anomcoupl_.Lambda2_z40 = (Hcouplings->H2zzLambda_qsq)[3][2];
    //
    for (int im=0; im<2; im++){
      spinzerohiggs_anomcoupl_.gh2z1[im] = (Hcouplings->H2zzcoupl)[0][im];
      spinzerohiggs_anomcoupl_.gh2z2[im] = (Hcouplings->H2zzcoupl)[1][im];
      spinzerohiggs_anomcoupl_.gh2z3[im] = (Hcouplings->H2zzcoupl)[2][im];
      spinzerohiggs_anomcoupl_.gh2z4[im] = (Hcouplings->H2zzcoupl)[3][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime[im] = (Hcouplings->H2zzcoupl)[10][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime2[im] = (Hcouplings->H2zzcoupl)[11][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime3[im] = (Hcouplings->H2zzcoupl)[12][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime4[im] = (Hcouplings->H2zzcoupl)[13][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime5[im] = (Hcouplings->H2zzcoupl)[14][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime[im] = (Hcouplings->H2zzcoupl)[15][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime2[im] = (Hcouplings->H2zzcoupl)[16][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime3[im] = (Hcouplings->H2zzcoupl)[17][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime4[im] = (Hcouplings->H2zzcoupl)[18][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime5[im] = (Hcouplings->H2zzcoupl)[19][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime[im] = (Hcouplings->H2zzcoupl)[20][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime2[im] = (Hcouplings->H2zzcoupl)[21][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime3[im] = (Hcouplings->H2zzcoupl)[22][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime4[im] = (Hcouplings->H2zzcoupl)[23][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime5[im] = (Hcouplings->H2zzcoupl)[24][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime[im] = (Hcouplings->H2zzcoupl)[25][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime2[im] = (Hcouplings->H2zzcoupl)[26][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime3[im] = (Hcouplings->H2zzcoupl)[27][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime4[im] = (Hcouplings->H2zzcoupl)[28][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime5[im] = (Hcouplings->H2zzcoupl)[29][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime6[im] = (Hcouplings->H2zzcoupl)[31][im];
      spinzerohiggs_anomcoupl_.gh2z1_prime7[im] = (Hcouplings->H2zzcoupl)[32][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime6[im] = (Hcouplings->H2zzcoupl)[33][im];
      spinzerohiggs_anomcoupl_.gh2z2_prime7[im] = (Hcouplings->H2zzcoupl)[34][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime6[im] = (Hcouplings->H2zzcoupl)[35][im];
      spinzerohiggs_anomcoupl_.gh2z3_prime7[im] = (Hcouplings->H2zzcoupl)[36][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime6[im] = (Hcouplings->H2zzcoupl)[37][im];
      spinzerohiggs_anomcoupl_.gh2z4_prime7[im] = (Hcouplings->H2zzcoupl)[38][im];
      //
      spinzerohiggs_anomcoupl_.gh2zgs1_prime2[im] = (Hcouplings->H2zzcoupl)[30][im];
      spinzerohiggs_anomcoupl_.gh2zgs2[im] = (Hcouplings->H2zzcoupl)[4][im];
      spinzerohiggs_anomcoupl_.gh2zgs3[im] = (Hcouplings->H2zzcoupl)[5][im];
      spinzerohiggs_anomcoupl_.gh2zgs4[im] = (Hcouplings->H2zzcoupl)[6][im];
      spinzerohiggs_anomcoupl_.gh2gsgs2[im] = (Hcouplings->H2zzcoupl)[7][im];
      spinzerohiggs_anomcoupl_.gh2gsgs3[im] = (Hcouplings->H2zzcoupl)[8][im];
      spinzerohiggs_anomcoupl_.gh2gsgs4[im] = (Hcouplings->H2zzcoupl)[9][im];
    }
    //
    if (spinzerohiggs_anomcoupl_.distinguish_HWWcouplings){
      //
      spinzerohiggs_anomcoupl_.c2w_q1sq = (Hcouplings->H2wwCLambda_qsq)[0];
      spinzerohiggs_anomcoupl_.Lambda2_w11 = (Hcouplings->H2wwLambda_qsq)[0][0];
      spinzerohiggs_anomcoupl_.Lambda2_w21 = (Hcouplings->H2wwLambda_qsq)[1][0];
      spinzerohiggs_anomcoupl_.Lambda2_w31 = (Hcouplings->H2wwLambda_qsq)[2][0];
      spinzerohiggs_anomcoupl_.Lambda2_w41 = (Hcouplings->H2wwLambda_qsq)[3][0];
      spinzerohiggs_anomcoupl_.c2w_q2sq = (Hcouplings->H2wwCLambda_qsq)[1];
      spinzerohiggs_anomcoupl_.Lambda2_w12 = (Hcouplings->H2wwLambda_qsq)[0][1];
      spinzerohiggs_anomcoupl_.Lambda2_w22 = (Hcouplings->H2wwLambda_qsq)[1][1];
      spinzerohiggs_anomcoupl_.Lambda2_w32 = (Hcouplings->H2wwLambda_qsq)[2][1];
      spinzerohiggs_anomcoupl_.Lambda2_w42 = (Hcouplings->H2wwLambda_qsq)[3][1];
      spinzerohiggs_anomcoupl_.c2w_q12sq = (Hcouplings->H2wwCLambda_qsq)[2];
      spinzerohiggs_anomcoupl_.Lambda2_w10 = (Hcouplings->H2wwLambda_qsq)[0][2];
      spinzerohiggs_anomcoupl_.Lambda2_w20 = (Hcouplings->H2wwLambda_qsq)[1][2];
      spinzerohiggs_anomcoupl_.Lambda2_w30 = (Hcouplings->H2wwLambda_qsq)[2][2];
      spinzerohiggs_anomcoupl_.Lambda2_w40 = (Hcouplings->H2wwLambda_qsq)[3][2];
      //
      for (int im=0; im<2; im++){
        spinzerohiggs_anomcoupl_.gh2w1[im] = (Hcouplings->H2wwcoupl)[0][im];
        spinzerohiggs_anomcoupl_.gh2w2[im] = (Hcouplings->H2wwcoupl)[1][im];
        spinzerohiggs_anomcoupl_.gh2w3[im] = (Hcouplings->H2wwcoupl)[2][im];
        spinzerohiggs_anomcoupl_.gh2w4[im] = (Hcouplings->H2wwcoupl)[3][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime[im] = (Hcouplings->H2wwcoupl)[10][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime2[im] = (Hcouplings->H2wwcoupl)[11][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime3[im] = (Hcouplings->H2wwcoupl)[12][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime4[im] = (Hcouplings->H2wwcoupl)[13][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime5[im] = (Hcouplings->H2wwcoupl)[14][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime[im] = (Hcouplings->H2wwcoupl)[15][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime2[im] = (Hcouplings->H2wwcoupl)[16][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime3[im] = (Hcouplings->H2wwcoupl)[17][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime4[im] = (Hcouplings->H2wwcoupl)[18][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime5[im] = (Hcouplings->H2wwcoupl)[19][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime[im] = (Hcouplings->H2wwcoupl)[20][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime2[im] = (Hcouplings->H2wwcoupl)[21][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime3[im] = (Hcouplings->H2wwcoupl)[22][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime4[im] = (Hcouplings->H2wwcoupl)[23][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime5[im] = (Hcouplings->H2wwcoupl)[24][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime[im] = (Hcouplings->H2wwcoupl)[25][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime2[im] = (Hcouplings->H2wwcoupl)[26][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime3[im] = (Hcouplings->H2wwcoupl)[27][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime4[im] = (Hcouplings->H2wwcoupl)[28][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime5[im] = (Hcouplings->H2wwcoupl)[29][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime6[im] = (Hcouplings->H2wwcoupl)[31][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime7[im] = (Hcouplings->H2wwcoupl)[32][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime6[im] = (Hcouplings->H2wwcoupl)[33][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime7[im] = (Hcouplings->H2wwcoupl)[34][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime6[im] = (Hcouplings->H2wwcoupl)[35][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime7[im] = (Hcouplings->H2wwcoupl)[36][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime6[im] = (Hcouplings->H2wwcoupl)[37][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime7[im] = (Hcouplings->H2wwcoupl)[38][im];
      }
    }
    else{
      //
      spinzerohiggs_anomcoupl_.c2w_q1sq = (Hcouplings->H2zzCLambda_qsq)[0];
      spinzerohiggs_anomcoupl_.Lambda2_w11 = (Hcouplings->H2zzLambda_qsq)[0][0];
      spinzerohiggs_anomcoupl_.Lambda2_w21 = (Hcouplings->H2zzLambda_qsq)[1][0];
      spinzerohiggs_anomcoupl_.Lambda2_w31 = (Hcouplings->H2zzLambda_qsq)[2][0];
      spinzerohiggs_anomcoupl_.Lambda2_w41 = (Hcouplings->H2zzLambda_qsq)[3][0];
      spinzerohiggs_anomcoupl_.c2w_q2sq = (Hcouplings->H2zzCLambda_qsq)[1];
      spinzerohiggs_anomcoupl_.Lambda2_w12 = (Hcouplings->H2zzLambda_qsq)[0][1];
      spinzerohiggs_anomcoupl_.Lambda2_w22 = (Hcouplings->H2zzLambda_qsq)[1][1];
      spinzerohiggs_anomcoupl_.Lambda2_w32 = (Hcouplings->H2zzLambda_qsq)[2][1];
      spinzerohiggs_anomcoupl_.Lambda2_w42 = (Hcouplings->H2zzLambda_qsq)[3][1];
      spinzerohiggs_anomcoupl_.c2w_q12sq = (Hcouplings->H2zzCLambda_qsq)[2];
      spinzerohiggs_anomcoupl_.Lambda2_w10 = (Hcouplings->H2zzLambda_qsq)[0][2];
      spinzerohiggs_anomcoupl_.Lambda2_w20 = (Hcouplings->H2zzLambda_qsq)[1][2];
      spinzerohiggs_anomcoupl_.Lambda2_w30 = (Hcouplings->H2zzLambda_qsq)[2][2];
      spinzerohiggs_anomcoupl_.Lambda2_w40 = (Hcouplings->H2zzLambda_qsq)[3][2];
      //
      for (int im=0; im<2; im++){
        spinzerohiggs_anomcoupl_.gh2w1[im] = (Hcouplings->H2zzcoupl)[0][im];
        spinzerohiggs_anomcoupl_.gh2w2[im] = (Hcouplings->H2zzcoupl)[1][im];
        spinzerohiggs_anomcoupl_.gh2w3[im] = (Hcouplings->H2zzcoupl)[2][im];
        spinzerohiggs_anomcoupl_.gh2w4[im] = (Hcouplings->H2zzcoupl)[3][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime[im] = (Hcouplings->H2zzcoupl)[10][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime2[im] = (Hcouplings->H2zzcoupl)[11][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime3[im] = (Hcouplings->H2zzcoupl)[12][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime4[im] = (Hcouplings->H2zzcoupl)[13][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime5[im] = (Hcouplings->H2zzcoupl)[14][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime[im] = (Hcouplings->H2zzcoupl)[15][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime2[im] = (Hcouplings->H2zzcoupl)[16][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime3[im] = (Hcouplings->H2zzcoupl)[17][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime4[im] = (Hcouplings->H2zzcoupl)[18][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime5[im] = (Hcouplings->H2zzcoupl)[19][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime[im] = (Hcouplings->H2zzcoupl)[20][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime2[im] = (Hcouplings->H2zzcoupl)[21][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime3[im] = (Hcouplings->H2zzcoupl)[22][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime4[im] = (Hcouplings->H2zzcoupl)[23][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime5[im] = (Hcouplings->H2zzcoupl)[24][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime[im] = (Hcouplings->H2zzcoupl)[25][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime2[im] = (Hcouplings->H2zzcoupl)[26][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime3[im] = (Hcouplings->H2zzcoupl)[27][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime4[im] = (Hcouplings->H2zzcoupl)[28][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime5[im] = (Hcouplings->H2zzcoupl)[29][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime6[im] = (Hcouplings->H2zzcoupl)[31][im];
        spinzerohiggs_anomcoupl_.gh2w1_prime7[im] = (Hcouplings->H2zzcoupl)[32][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime6[im] = (Hcouplings->H2zzcoupl)[33][im];
        spinzerohiggs_anomcoupl_.gh2w2_prime7[im] = (Hcouplings->H2zzcoupl)[34][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime6[im] = (Hcouplings->H2zzcoupl)[35][im];
        spinzerohiggs_anomcoupl_.gh2w3_prime7[im] = (Hcouplings->H2zzcoupl)[36][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime6[im] = (Hcouplings->H2zzcoupl)[37][im];
        spinzerohiggs_anomcoupl_.gh2w4_prime7[im] = (Hcouplings->H2zzcoupl)[38][im];
      }
    }
    /***** END SECOND RESONANCE *****/
  }
}
void TUtil::SetJHUGenSpinZeroVVCouplings(double Hvvcoupl[SIZE_HVV][2], int Hvvcoupl_cqsq[3], double HvvLambda_qsq[4][3], bool useWWcoupl){
  const double GeV = 1./100.;
  int iWWcoupl = (useWWcoupl ? 1 : 0);
  for (int c=0; c<4; c++){ for (int k=0; k<3; k++) HvvLambda_qsq[c][k] *= GeV; } // GeV units in JHUGen
  __modjhugenmela_MOD_setspinzerovvcouplings(Hvvcoupl, Hvvcoupl_cqsq, HvvLambda_qsq, &iWWcoupl);
}
void TUtil::SetJHUGenSpinZeroGGCouplings(double Hggcoupl[SIZE_HGG][2]){ __modjhugenmela_MOD_setspinzeroggcouplings(Hggcoupl); }
void TUtil::SetJHUGenSpinZeroQQCouplings(double Hqqcoupl[SIZE_HQQ][2]){ __modjhugenmela_MOD_setspinzeroqqcouplings(Hqqcoupl); }
void TUtil::SetJHUGenSpinOneCouplings(double Zqqcoupl[SIZE_ZQQ][2], double Zvvcoupl[SIZE_ZVV][2]){ __modjhugenmela_MOD_setspinonecouplings(Zqqcoupl, Zvvcoupl); }
void TUtil::SetJHUGenSpinTwoCouplings(double Gacoupl[SIZE_GGG][2], double Gbcoupl[SIZE_GVV][2], double qLeftRightcoupl[SIZE_GQQ][2]){ __modjhugenmela_MOD_setspintwocouplings(Gacoupl, Gbcoupl, qLeftRightcoupl); }

bool TUtil::MCFM_masscuts(double s[][mxpart], TVar::Process process){
  double minZmassSqr=10*10;
  if (
    process==TVar::bkgZZ
    &&
    (s[2][3]<minZmassSqr || s[4][5]<minZmassSqr)
    ) return true;
  return false;
}
bool TUtil::MCFM_smalls(double s[][mxpart], int npart){

  // Reject event if any s(i,j) is too small
  // cutoff is defined in technical.Dat

  if (
    npart == 3 &&
    (
    (-s[5-1][1-1]< cutoff_.cutoff)  //gamma p1
    || (-s[5-1][2-1]< cutoff_.cutoff)  //gamma p2
    || (-s[4-1][1-1]< cutoff_.cutoff)  //e+    p1
    || (-s[4-1][2-1]< cutoff_.cutoff)  //e-    p2
    || (-s[3-1][1-1]< cutoff_.cutoff)  //nu    p1
    || (-s[3-1][2-1]< cutoff_.cutoff)  //nu    p2
    || (+s[5-1][4-1]< cutoff_.cutoff)  //gamma e+
    || (+s[5-1][3-1]< cutoff_.cutoff)  //gamma nu
    || (+s[4-1][3-1]< cutoff_.cutoff)  //e+    nu
    )
    )
    return true;

  else if (
    npart == 4 &&
    (
    (-s[5-1][1-1]< cutoff_.cutoff)  //e-    p1
    || (-s[5-1][2-1]< cutoff_.cutoff)  //e-    p2
    || (-s[6-1][1-1]< cutoff_.cutoff)  //nb    p1
    || (-s[6-1][2-1]< cutoff_.cutoff)  //nb    p2
    || (+s[6-1][5-1]< cutoff_.cutoff)  //e-    nb
    )

    )

    return true;

  return false;
}

//Make sure
// 1. tot Energy Sum < 2EBEAM
// 2. PartonEnergy Fraction minimum<x0,x1<1
// 3. number of final state particle is defined
//
double TUtil::SumMatrixElementPDF(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, MelaIO* RcdME,
  double EBEAM,
  double coupling[SIZE_HVV_FREENORM], // This last argument is unfortunately the simplest way to pass these couplings
  TVar::VerbosityLevel verbosity
  ){

  int partIncCode=TVar::kNoAssociated; // Do not use associated particles in the pT=0 frame boost
  int nRequested_AssociatedJets = 0;
  if (
    production==TVar::JJVBF
    &&
    (
    (
    process==TVar::HSMHiggs
    ||
    process==TVar::bkgZZ_SMHiggs
    ||
    process==TVar::bkgZZ
    ||
    process==TVar::bkgWW_SMHiggs
    ||
    process==TVar::bkgWW
    )
    )
    ){ // Use asociated jets in the pT=0 frame boost
    partIncCode=TVar::kUseAssociated_Jets;
    nRequested_AssociatedJets = 2;
  }
  simple_event_record mela_event;
  mela_event.AssociationCode=partIncCode;
  mela_event.nRequested_AssociatedJets=nRequested_AssociatedJets;
  GetBoostedParticleVectors(
    RcdME->melaCand,
    mela_event
    );

  double xx[2]={ 0 };
  if (!CheckPartonMomFraction(mela_event.pMothers.at(0).second, mela_event.pMothers.at(1).second, xx, EBEAM, TVar::ERROR)) return 0;
  if (xx[0]==0. || xx[1]==0. || EBEAM==0) return 0;

  TLorentzVector MomStore[mxpart];
  for (int i = 0; i < mxpart; i++) MomStore[i].SetXYZT(0, 0, 0, 0);

  int NPart=npart_.npart+2;
  double p4[4][mxpart] = { { 0 } };
  double s[mxpart][mxpart] = { { 0 } };
  double msq[nmsq][nmsq];
  double msqjk=0;
  int channeltoggle=0;

  //Convert TLorentzVector into 4xNPart Matrix
  //reverse sign of incident partons
  for(int ipar=0;ipar<2;ipar++){
    if(mela_event.pMothers.at(ipar).second.T()>0.){
      p4[0][ipar] = -mela_event.pMothers.at(ipar).second.X();
      p4[1][ipar] = -mela_event.pMothers.at(ipar).second.Y();
      p4[2][ipar] = -mela_event.pMothers.at(ipar).second.Z();
      p4[3][ipar] = -mela_event.pMothers.at(ipar).second.T();
      MomStore[ipar] = mela_event.pMothers.at(ipar).second;
    }
    else{
      p4[0][ipar] = mela_event.pMothers.at(ipar).second.X();
      p4[1][ipar] = mela_event.pMothers.at(ipar).second.Y();
      p4[2][ipar] = mela_event.pMothers.at(ipar).second.Z();
      p4[3][ipar] = mela_event.pMothers.at(ipar).second.T();
      MomStore[ipar] = -mela_event.pMothers.at(ipar).second;
    }
  }

  // Determine if the decay mode involves WW or ZZ, to be used for ZZ or WW-specific signal MEs
  //bool isZG = (PDGHelpers::isAZBoson(mela_event.intermediateVid.at(0)) && PDGHelpers::isAPhoton(mela_event.intermediateVid.at(1)));
  bool isWW = (PDGHelpers::isAWBoson(mela_event.intermediateVid.at(0)) && PDGHelpers::isAWBoson(mela_event.intermediateVid.at(1)));
  bool isZZ = (PDGHelpers::isAZBoson(mela_event.intermediateVid.at(0)) && PDGHelpers::isAZBoson(mela_event.intermediateVid.at(1)));
  //initialize decayed particles
  for (int ipar=2; ipar<min(NPart, (int)(mela_event.pDaughters.size()+mela_event.pAssociated.size())+2); ipar++){
    TLorentzVector* momTmp;
    if (ipar<(int)(mela_event.pDaughters.size())+2) momTmp=&(mela_event.pDaughters.at(ipar-2).second);
    else momTmp=&(mela_event.pAssociated.at(ipar-2).second);
    p4[0][ipar] = momTmp->X();
    p4[1][ipar] = momTmp->Y();
    p4[2][ipar] = momTmp->Z();
    p4[3][ipar] = momTmp->T();
    MomStore[ipar]=*momTmp;
  }

  if (verbosity >= TVar::DEBUG){
    for (int i=0; i<NPart; i++) cout << "p["<<i<<"] (Px, Py, Pz, E):\t" << p4[0][i] << '\t' << p4[1][i] << '\t' << p4[2][i] << '\t' << p4[3][i] << endl;
  }

  double defaultRenScale = scale_.scale;
  double defaultFacScale = facscale_.facscale;
//  cout << "Default scales: " << defaultRenScale << '\t' << defaultFacScale << endl;
  int defaultNloop = nlooprun_.nlooprun;
  int defaultNflav = nflav_.nflav;
  string defaultPdflabel = pdlabel_.pdlabel;
  double renQ = InterpretScaleScheme(production, matrixElement, event_scales->renomalizationScheme, MomStore);
//  cout << "renQ: " << renQ << " x " << event_scales->ren_scale_factor << endl;
  double facQ = InterpretScaleScheme(production, matrixElement, event_scales->factorizationScheme, MomStore);
//  cout << "facQ: " << facQ << " x " << event_scales->fac_scale_factor << endl;
  SetAlphaS(renQ, facQ, event_scales->ren_scale_factor, event_scales->fac_scale_factor, 1, 5, "cteq6_l"); // Set AlphaS(|Q|/2, mynloop, mynflav, mypartonPDF) for MCFM ME-related calculations

  //calculate invariant masses between partons/final state particles
  for (int jdx=0; jdx< NPart; jdx++){
    s[jdx][jdx]=0;
    for (int kdx=jdx+1; kdx<NPart; kdx++){
      s[jdx][kdx]=2*(p4[3][jdx]*p4[3][kdx]-p4[2][jdx]*p4[2][kdx]-p4[1][jdx]*p4[1][kdx]-p4[0][jdx]*p4[0][kdx]);
      s[kdx][jdx]=s[jdx][kdx];
    }
  }

  bool passMassCuts=true;
  if (passMassCuts){
    if ((production == TVar::ZZQQB_STU || production == TVar::ZZQQB_S || production == TVar::ZZQQB_TU) && process == TVar::bkgZZ){
      if (production == TVar::ZZQQB_STU) channeltoggle=0;
      else if (production == TVar::ZZQQB_S) channeltoggle=1;
      else/* if (production == TVar::ZZQQB_TU)*/ channeltoggle=2;
      qqb_zz_stu_(p4[0], msq[0], &channeltoggle);

      msqjk = msq[3][7] + msq[7][3]; // all of the unweighted MEs are the same
      SumMEPDF(MomStore[0], MomStore[1], msq, RcdME, EBEAM, verbosity);
    }
    else if (production == TVar::ZZINDEPENDENT || production == TVar::ZZQQB){
      if (process == TVar::bkgZJJ) qqb_z2jet_(p4[0], msq[0]);
      else if (process == TVar::bkgZGamma) qqb_zgam_(p4[0], msq[0]);
      else if (process == TVar::bkgZZ) qqb_zz_(p4[0], msq[0]);
      else if (process == TVar::bkgWW) qqb_ww_(p4[0], msq[0]);

      msqjk = msq[3][7] + msq[7][3]; // all of the unweighted MEs are the same, take uub
      SumMEPDF(MomStore[0], MomStore[1], msq, RcdME, EBEAM, verbosity);
    }
    // the subroutine for the calculations including the interfenrence
    // ME =  sig + inter (sign, bkg)
    else if(production == TVar::ZZGG){
      if (process==TVar::bkgZZ_SMHiggs && matrixElement==TVar::JHUGen) gg_zz_int_freenorm_(p4[0], coupling, msq[0]); // |ggZZ + ggHZZ|**2
      else if (isZZ){
        if (process==TVar::HSMHiggs) gg_hzz_tb_(p4[0], msq[0]); // |ggHZZ|**2
        else if (process==TVar::bkgZZ_SMHiggs) gg_zz_all_(p4[0], msq[0]); // |ggZZ + ggHZZ|**2
        else if (process==TVar::bkgZZ) gg_zz_(p4[0], &(msq[5][5])); // |ggZZ|**2
        else if (process==TVar::HSMHiggs_ZZWW) gg_hvv_tb_(p4[0], msq[0]); // |ggHZZ+WW|**2
        else if (process==TVar::bkgWWZZ_SMHiggs) gg_vv_all_(p4[0], msq[0]); // |ggZZ + ggHZZ (+WW)|**2
        else if (process==TVar::bkgWWZZ) gg_vv_(p4[0], &(msq[5][5])); // |ggZZ+WW|**2
      }
      else if (isWW){
        if (process==TVar::HSMHiggs || process==TVar::HSMHiggs_ZZWW){
          for (unsigned int ix=0; ix<4; ix++) swap(p4[ix][2], p4[ix][4]); // Vectors are passed with ZZ as basis
          gg_hvv_tb_(p4[0], msq[0]); // |ggHZZ+WW|**2
        }
        else if (process==TVar::bkgWW_SMHiggs || process==TVar::bkgWWZZ_SMHiggs){
          for (unsigned int ix=0; ix<4; ix++) swap(p4[ix][2], p4[ix][4]); // Vectors are passed with ZZ as basis
          gg_vv_all_(p4[0], msq[0]); // |ggZZ + ggHZZ (+WW)|**2
        }
        else if (process==TVar::bkgWW || process==TVar::bkgWWZZ){
          for (unsigned int ix=0; ix<4; ix++) swap(p4[ix][2], p4[ix][4]); // Vectors are passed with ZZ as basis
          gg_vv_(p4[0], &(msq[5][5])); // |ggZZ+WW|**2
        }
      }

      msqjk = msq[5][5]; // gg-only
      SumMEPDF(MomStore[0], MomStore[1], msq, RcdME, EBEAM, verbosity);
    }
    else if (production == TVar::JJVBF){
      if (isZZ && (process==TVar::bkgZZ_SMHiggs || process==TVar::HSMHiggs || process==TVar::bkgZZ)) qq_zzqq_(p4[0], msq[0]); // VBF MCFM SBI, S or B
      // FIXME
      //else if (isWW && (process==TVar::bkgWW_SMHiggs || process==TVar::HSMHiggs || process==TVar::bkgWW)) qq_wwqq_(p4[0], msq[0]); // VBF MCFM SBI, S or B

      msqjk = SumMEPDF(MomStore[0], MomStore[1], msq, RcdME, EBEAM, verbosity);
    }
  }

  if (msqjk != msqjk){
    cout << "SumMatrixPDF: "<< TVar::ProcessName(process) << " msqjk="  << msqjk << endl;
    msqjk=0;
  }

//  cout << "Before reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  SetAlphaS(defaultRenScale, defaultFacScale, 1., 1., defaultNloop, defaultNflav, defaultPdflabel); // Protection for other probabilities
//  cout << "Default scale reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  return msqjk;
}


double TUtil::JHUGenMatEl(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, MelaIO* RcdME,
  double EBEAM,
  TVar::VerbosityLevel verbosity
  ){
  const double GeV=1./100.; // JHUGen mom. scale factor
  double MatElSq=0; // Return value

  if (matrixElement!=TVar::JHUGen){ cerr << "TUtil::JHUGenMatEl: Non-JHUGen MEs are not supported" << endl; return MatElSq; }
  bool isSpinZero = (
    process == TVar::HSMHiggs
    || process == TVar::H0minus
    || process == TVar::H0hplus
    || process == TVar::H0_g1prime2
    || process== TVar::H0_Zgs
    || process ==TVar::H0_gsgs
    || process ==TVar::H0_Zgs_PS
    || process ==TVar::H0_gsgs_PS
    || process ==TVar::H0_Zgsg1prime2
    || process == TVar::SelfDefine_spin0
    );
  bool isSpinOne = (
    process == TVar::H1minus
    || process == TVar::H1plus
    || process == TVar::SelfDefine_spin1
    );
  bool isSpinTwo = (
    process == TVar::H2_g1g5
    || process == TVar::H2_g1
    || process == TVar::H2_g8
    || process == TVar::H2_g4
    || process == TVar::H2_g5
    || process == TVar::H2_g2
    || process == TVar::H2_g3
    || process == TVar::H2_g6
    || process == TVar::H2_g7
    || process == TVar::H2_g9
    || process == TVar::H2_g10
    || process == TVar::SelfDefine_spin2
    );
  if (!(isSpinZero || isSpinOne || isSpinTwo)){ cerr << "TUtil::JHUGenMatEl: Process " << process << " not supported." << endl; return MatElSq; }

  double msq[nmsq][nmsq]={ { 0 } }; // ME**2[parton2][parton1] for each incoming parton 1 and 2, used in RcdME
  int MYIDUP_tmp[4]={ 0 }; // Initial assignment array, unconverted. 0==Unassigned
  vector<pair<int, int>> idarray[2]; // All possible ids for each daughter based on the value of MYIDUP_tmp[0:3] and the desired V ids taken from mela_event.intermediateVid.at(0:1)
  int MYIDUP[4]={ 0 }; // "Working" assignment, converted
  int idfirst[2]={ 0 }; // Used to set DecayMode1, = MYIDUP[0:1]
  int idsecond[2]={ 0 }; // Used to set DecayMode2, = MYIDUP[2:3]
  double p4[6][4]={ { 0 } }; // Mom (*GeV) to pass into JHUGen
  TLorentzVector MomStore[mxpart]; // Mom (in natural units) to compute alphaS
  for (int i = 0; i < mxpart; i++) MomStore[i].SetXYZT(0, 0, 0, 0);

  // Notice that partIncCode is specific for this subroutine
  int partIncCode=TVar::kNoAssociated; // Do not use associated particles in the pT=0 frame boost
  simple_event_record mela_event;
  mela_event.AssociationCode=partIncCode;
  GetBoostedParticleVectors(
    RcdME->melaCand,
    mela_event
    );
  if (mela_event.pDaughters.size()<2 || mela_event.intermediateVid.size()!=2){
    cerr << "TUtil::JHUGenMatEl: Number of daughters " << mela_event.pDaughters.size() << " or number of intermediate Vs " << mela_event.intermediateVid.size() << " not supported!" << endl;
    return MatElSq;
  }

  // p(i,0:3) = (E(i),px(i),py(i),pz(i))
  // i=0,1: g1,g2 or q1, qb2 (outgoing convention)
  // i=2,3: correspond to MY_IDUP(0),MY_IDUP(1)
  // i=4,5: correspond to MY_IDUP(2),MY_IDUP(3)
  for (int ipar=0; ipar<2; ipar++){
    if (mela_event.pMothers.at(ipar).second.T()>0.){
      p4[ipar][0] = -mela_event.pMothers.at(ipar).second.T()*GeV;
      p4[ipar][1] = -mela_event.pMothers.at(ipar).second.X()*GeV;
      p4[ipar][2] = -mela_event.pMothers.at(ipar).second.Y()*GeV;
      p4[ipar][3] = -mela_event.pMothers.at(ipar).second.Z()*GeV;
      MomStore[ipar] = mela_event.pMothers.at(ipar).second;
    }
    else{
      p4[ipar][0] = mela_event.pMothers.at(ipar).second.T()*GeV;
      p4[ipar][1] = mela_event.pMothers.at(ipar).second.X()*GeV;
      p4[ipar][2] = mela_event.pMothers.at(ipar).second.Y()*GeV;
      p4[ipar][3] = mela_event.pMothers.at(ipar).second.Z()*GeV;
      MomStore[ipar] = -mela_event.pMothers.at(ipar).second;
    }
    // From Markus:
    // Note that the momentum no.2, p(1:4, 2), is a dummy which is not used in case production==TVar::ZZINDEPENDENT.
    if (ipar==1 && production==TVar::ZZINDEPENDENT){ for (int ix=0; ix<4; ix++){ p4[0][ix] += p4[ipar][ix]; p4[ipar][ix]=0.; } }
  }
  //initialize decayed particles
  if (mela_event.pDaughters.size()==2){
    for (unsigned int ipar=0; ipar<mela_event.pDaughters.size(); ipar++){
      TLorentzVector* momTmp = &(mela_event.pDaughters.at(ipar).second);
      int* idtmp = &(mela_event.pDaughters.at(ipar).first);

      int arrindex = ipar;
      if (PDGHelpers::isAPhoton(*idtmp) && ipar==1) arrindex=2; // In GG
      if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_tmp[arrindex] = *idtmp;
      else MYIDUP_tmp[ipar] = 0;
      p4[arrindex+2][0] = momTmp->T()*GeV;
      p4[arrindex+2][1] = momTmp->X()*GeV;
      p4[arrindex+2][2] = momTmp->Y()*GeV;
      p4[arrindex+2][3] = momTmp->Z()*GeV;
      MomStore[arrindex+2] = *momTmp;
    }
  }
  else{
    for (unsigned int ipar=0; ipar<4; ipar++){
      if (ipar<mela_event.pDaughters.size()){
        TLorentzVector* momTmp = &(mela_event.pDaughters.at(ipar).second);
        int* idtmp = &(mela_event.pDaughters.at(ipar).first);

        if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_tmp[ipar] = *idtmp;
        else MYIDUP_tmp[ipar] = 0;
        p4[ipar+2][0] = momTmp->T()*GeV;
        p4[ipar+2][1] = momTmp->X()*GeV;
        p4[ipar+2][2] = momTmp->Y()*GeV;
        p4[ipar+2][3] = momTmp->Z()*GeV;
        MomStore[ipar+2] = *momTmp;
      }
      else MYIDUP_tmp[ipar] = -9000; // No need to set p4, which is already 0 by initialization
      // __modparameters_MOD_not_a_particle__?
      if (verbosity >= TVar::DEBUG) cout << "MYIDUP_tmp[" << ipar << "]=" << MYIDUP_tmp[ipar] << endl;
    }
  }

  // Set alphas
  double defaultRenScale = scale_.scale;
  double defaultFacScale = facscale_.facscale;
  //  cout << "Default scales: " << defaultRenScale << '\t' << defaultFacScale << endl;
  int defaultNloop = nlooprun_.nlooprun;
  int defaultNflav = nflav_.nflav;
  string defaultPdflabel = pdlabel_.pdlabel;
  double renQ = InterpretScaleScheme(production, matrixElement, event_scales->renomalizationScheme, MomStore);
  //  cout << "renQ: " << renQ << " x " << event_scales->ren_scale_factor << endl;
  double facQ = InterpretScaleScheme(production, matrixElement, event_scales->factorizationScheme, MomStore);
  //  cout << "facQ: " << facQ << " x " << event_scales->fac_scale_factor << endl;
  SetAlphaS(renQ, facQ, event_scales->ren_scale_factor, event_scales->fac_scale_factor, 1, 5, "cteq6_l"); // Set AlphaS(|Q|/2, mynloop, mynflav, mypartonPDF)

  // Determine te actual ids to compute the ME. Assign ids if any are unknown.
  for (int iv=0; iv<2; iv++){ // Max. 2 vector bosons
    if (MYIDUP_tmp[2*iv+0]!=0 && MYIDUP_tmp[2*iv+1]!=0){ // Z->2l,2nu,2q, W->lnu,qq', G
      // OSSF pairs or just one "V-daughter"
      if (
        TMath::Sign(1, MYIDUP_tmp[2*iv+0])==TMath::Sign(1, -MYIDUP_tmp[2*iv+1])
        ||
        (MYIDUP_tmp[2*iv+0]==-9000 || MYIDUP_tmp[2*iv+1]==-9000)
        ) idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], MYIDUP_tmp[2*iv+1]));
      // SSSF pairs, ordered already by phi, so avoid the re-ordering inside the ME
      else if (MYIDUP_tmp[2*iv+0]<0) idarray[iv].push_back(pair<int, int>(-MYIDUP_tmp[2*iv+0], MYIDUP_tmp[2*iv+1])); // Reverse sign of first daughter if SS(--)SF pair
      else idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], -MYIDUP_tmp[2*iv+1])); // Reverse sign of daughter if SS(++)SF pair
    }
    else if (MYIDUP_tmp[2*iv+0]!=0){ // ->f?,  one quark is known
      if (PDGHelpers::isAWBoson(mela_event.intermediateVid.at(iv))){ // (W+/-)->f?
        if (PDGHelpers::isUpTypeQuark(MYIDUP_tmp[2*iv+0])){ // (W+)->u?
          int id_dn = TMath::Sign(1, -MYIDUP_tmp[2*iv+0]);
          int id_st = TMath::Sign(3, -MYIDUP_tmp[2*iv+0]);
          int id_bt = TMath::Sign(5, -MYIDUP_tmp[2*iv+0]);
          idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], id_dn));
          idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], id_st));
          idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], id_bt));
        }
        else if (PDGHelpers::isDownTypeQuark(MYIDUP_tmp[2*iv+0])){ // (W-)->d?
          int id_up = TMath::Sign(2, -MYIDUP_tmp[2*iv+0]);
          int id_ch = TMath::Sign(4, -MYIDUP_tmp[2*iv+0]);
          idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], id_up));
          idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], id_ch));
        }
      }
      else if (PDGHelpers::isAZBoson(mela_event.intermediateVid.at(iv))){ // Z->f?
        idarray[iv].push_back(pair<int, int>(MYIDUP_tmp[2*iv+0], -MYIDUP_tmp[2*iv+0]));
      }
    }
    else if (MYIDUP_tmp[2*iv+1]!=0){ // ->?fb,  one quark is known
      if (PDGHelpers::isAWBoson(mela_event.intermediateVid.at(iv))){ // (W+/-)->?fb
        if (PDGHelpers::isUpTypeQuark(MYIDUP_tmp[2*iv+1])){ // (W-)->?ub
          int id_dn = TMath::Sign(1, -MYIDUP_tmp[2*iv+1]);
          int id_st = TMath::Sign(3, -MYIDUP_tmp[2*iv+1]);
          int id_bt = TMath::Sign(5, -MYIDUP_tmp[2*iv+1]);
          idarray[iv].push_back(pair<int, int>(id_dn, MYIDUP_tmp[2*iv+1]));
          idarray[iv].push_back(pair<int, int>(id_st, MYIDUP_tmp[2*iv+1]));
          idarray[iv].push_back(pair<int, int>(id_bt, MYIDUP_tmp[2*iv+1]));
        }
        else if (PDGHelpers::isDownTypeQuark(MYIDUP_tmp[2*iv+1])){ // (W+)->?db
          int id_up = TMath::Sign(2, -MYIDUP_tmp[2*iv+1]);
          int id_ch = TMath::Sign(4, -MYIDUP_tmp[2*iv+1]);
          idarray[iv].push_back(pair<int, int>(id_up, MYIDUP_tmp[2*iv+1]));
          idarray[iv].push_back(pair<int, int>(id_ch, MYIDUP_tmp[2*iv+1]));
        }
      }
      else if (PDGHelpers::isAZBoson(mela_event.intermediateVid.at(iv))){ // Z->?fb
        idarray[iv].push_back(pair<int, int>(-MYIDUP_tmp[2*iv+1], MYIDUP_tmp[2*iv+1]));
      }
    }
    else{ // Both fermions unknown
      if (PDGHelpers::isAWBoson(mela_event.intermediateVid.at(iv))){ // (W+/-)->??
        for (int iqu=1; iqu<=2; iqu++){
          int id_up = iqu*2;
          for (int iqd=1; iqd<=3; iqd++){
            int id_dn = iqd*2-1;
            if (iv==0){ idarray[iv].push_back(pair<int, int>(id_up, -id_dn)); idarray[iv].push_back(pair<int, int>(-id_dn, id_up)); }
            else{ idarray[iv].push_back(pair<int, int>(id_dn, -id_up)); idarray[iv].push_back(pair<int, int>(-id_up, id_dn)); }
          }
        }
      }
      else if (PDGHelpers::isAZBoson(mela_event.intermediateVid.at(iv))){ // Z->??
        for (int iq=1; iq<=5; iq++){ idarray[iv].push_back(pair<int, int>(iq, -iq)); idarray[iv].push_back(pair<int, int>(-iq, iq)); }
      }
    }
  }

  int nNonZero=0;
  for (unsigned int v1=0; v1<idarray[0].size(); v1++){
    for (unsigned int v2=0; v2<idarray[1].size(); v2++){
      // Convert the particle ids to JHU convention
      MYIDUP[0] = convertLHEreverse(&(idarray[0].at(v1).first));
      MYIDUP[1] = convertLHEreverse(&(idarray[0].at(v1).second));
      MYIDUP[2] = convertLHEreverse(&(idarray[1].at(v2).first));
      MYIDUP[3] = convertLHEreverse(&(idarray[1].at(v2).second));

      // Check working ids
      if (verbosity>=TVar::DEBUG){ for (unsigned int idau=0; idau<4; idau++) cout << "MYIDUP[" << idau << "]=" << MYIDUP[idau] << endl; }

      // Determine M_V and Ga_V in JHUGen, needed for g1 vs everything else.
      for (int ip=0; ip<2; ip++){ idfirst[ip]=MYIDUP[ip]; idsecond[ip]=MYIDUP[ip+2]; }
      __modjhugenmela_MOD_setdecaymodes(idfirst, idsecond); // Set M_V and Ga_V in JHUGen

      double MatElTmp=0.;
      if (production == TVar::ZZGG){
        if (isSpinZero) __modhiggs_MOD_evalamp_gg_h_vv(p4, MYIDUP, &MatElTmp);
        else if (isSpinTwo) __modgraviton_MOD_evalamp_gg_g_vv(p4, MYIDUP, &MatElTmp);
      }
      else if (production == TVar::ZZQQB){
        if (isSpinOne) __modzprime_MOD_evalamp_qqb_zprime_vv(p4, MYIDUP, &MatElTmp);
        else if (isSpinTwo) __modgraviton_MOD_evalamp_qqb_g_vv(p4, MYIDUP, &MatElTmp);
      }
      else if (production == TVar::ZZINDEPENDENT){
        if (isSpinZero) __modhiggs_MOD_evalamp_h_vv(p4, MYIDUP, &MatElTmp);
        else if (isSpinOne) __modzprime_MOD_evalamp_zprime_vv(p4, MYIDUP, &MatElTmp);
        else if (isSpinTwo) __modgraviton_MOD_evalamp_g_vv(p4, MYIDUP, &MatElTmp);
      }
      // Add CKM elements since they are not included
      if (PDGHelpers::isAWBoson(mela_event.intermediateVid.at(0))) MatElTmp *= pow(__modparameters_MOD_ckm(&(idarray[0].at(v1).first), &(idarray[0].at(v1).second))/__modparameters_MOD_scalefactor(&(idarray[0].at(v1).first), &(idarray[0].at(v1).second)), 2);
      if (PDGHelpers::isAWBoson(mela_event.intermediateVid.at(1))) MatElTmp *= pow(__modparameters_MOD_ckm(&(idarray[1].at(v2).first), &(idarray[1].at(v2).second))/__modparameters_MOD_scalefactor(&(idarray[1].at(v2).first), &(idarray[1].at(v2).second)), 2);

      if (verbosity >= TVar::DEBUG) cout << "TUtil::JHUGenMatEl: Instance MatElTmp = " << MatElTmp << '\n' << endl;
      MatElSq += MatElTmp;
      if (MatElTmp>0.) nNonZero++;
    }
  }
  if (nNonZero>0) MatElSq /= ((double)nNonZero);
  if (verbosity >= TVar::DEBUG){
    cout << "TUtil::JHUGenMatEl: Number of matrix element instances computed: " << nNonZero << endl;
    cout << "TUtil::JHUGenMatEl: MatElSq after division = " << MatElSq << endl;
  }
  // This constant is needed to account for the different units used in
  // JHUGen compared to the MCFM
  double constant = 1.45e-8;
  if (isSpinZero && production!=TVar::ZZINDEPENDENT) constant = 4.46162946e-4;
  // == 1.45e-8/pow(0.13229060/(3.*3.141592653589793238462643383279502884197*2.4621845810181631), 2), constant was 1.45e-8 before with alpha_s=0.13229060, vev=2.4621845810181631/GeV
  MatElSq *= constant;

  // Set RcdME information for ME and parton distributions, taking into account the mothers if id!=0 (i.e. if not unknown).
  if (mela_event.pMothers.at(0).first!=0 && mela_event.pMothers.at(1).first!=0){
    int ix=0, iy=0;
    if (abs(mela_event.pMothers.at(0).first)<6) ix=mela_event.pMothers.at(0).first;
    if (abs(mela_event.pMothers.at(1).first)<6) iy=mela_event.pMothers.at(1).first;
    msq[iy][ix]=MatElSq; // Note that SumMEPdf receives a transposed msq
  }
  else{
    if (production == TVar::ZZGG || production == TVar::ZZINDEPENDENT) msq[5][5]=MatElSq;
    else if (production == TVar::ZZQQB){
      for (int ix=0; ix<5; ix++){ msq[ix][10-ix]=MatElSq; msq[10-ix][ix]=MatElSq; }
    }
  }
  if (production!=TVar::ZZINDEPENDENT) SumMEPDF(MomStore[0], MomStore[1], msq, RcdME, EBEAM, verbosity);
  else{ // If production is ZZINDEPENDENT, only set gg index with fx1,2[g,g]=1.
    double fx_dummy[nmsq]={ 0 }; fx_dummy[5]=1.;
    RcdME->setPartonWeights(fx_dummy, fx_dummy);
    RcdME->setMEArray(msq, true);
    RcdME->computeWeightedMEArray();
  }

  if (verbosity >= TVar::DEBUG) cout << "TUtil::JHUGenMatEl: Final MatElSq = " << MatElSq << endl;

  // Reset alphas
  SetAlphaS(defaultRenScale, defaultFacScale, 1., 1., defaultNloop, defaultNflav, defaultPdflabel); // Protection for other probabilities
  //cout << "TUtil::MatElSq = " << MatElSq << endl;
  return MatElSq;
}

double TUtil::HJJMatEl(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, MelaIO* RcdME,
  double EBEAM,
  TVar::VerbosityLevel verbosity
  ){
  const double GeV=1./100.; // JHUGen mom. scale factor
  double sum_msqjk = 0;
  // by default assume only gg productions
  // FOTRAN convention -5    -4   -3  -2    -1  0 1 2 3 4 5
  //     parton flavor bbar cbar sbar ubar dbar g d u s c b
  // C++ convention     0     1   2    3    4   5 6 7 8 9 10
  //2-D matrix is reversed in fortran
  // msq[ parton2 ] [ parton1 ]
  //      flavor_msq[jj][ii] = fx1[ii]*fx2[jj]*msq[jj][ii];
  double MatElsq[nmsq][nmsq]={ { 0 } };
  double MatElsq_tmp[nmsq][nmsq]={ { 0 } };

  if (matrixElement!=TVar::JHUGen){ cerr << "TUtil::HJJMatEl: Non-JHUGen MEs are not supported" << endl; return sum_msqjk; }
  if (!(production==TVar::JJGG || production==TVar::JJVBF || production==TVar::JH)){ cerr << "TUtil::HJJMatEl: Production is not supported!" << endl; return sum_msqjk; }

  // Notice that partIncCode is specific for this subroutine
  int nRequested_AssociatedJets=2;
  if (production == TVar::JH) nRequested_AssociatedJets=1;
  int partIncCode=TVar::kUseAssociated_Jets; // Only use associated partons in the pT=0 frame boost
  simple_event_record mela_event;
  mela_event.AssociationCode=partIncCode;
  mela_event.nRequested_AssociatedJets=nRequested_AssociatedJets;
  GetBoostedParticleVectors(
    RcdME->melaCand,
    mela_event
    );
  if (mela_event.pAssociated.size()==0){ cerr << "TUtil::HJJMatEl: Number of associated particles is 0!" << endl; return sum_msqjk; }

  int MYIDUP_tmp[4]={ 0 }; // "Incoming" partons 1, 2, "outgoing" partons 3, 4
  double p4[5][4]={ { 0 } };
  double pOneJet[4][4] ={ { 0 } }; // For HJ
  TLorentzVector MomStore[mxpart];
  for (int i = 0; i < mxpart; i++) MomStore[i].SetXYZT(0, 0, 0, 0);

  // p4(0:4,i) = (E(i),px(i),py(i),pz(i))
  // i=0,1: g1,g2 or q1, qb2 (outgoing convention)
  // i=2,3: J1, J2 in outgoing convention when possible
  // i=4: H
  for (int ipar=0; ipar<2; ipar++){
    TLorentzVector* momTmp = &(mela_event.pMothers.at(ipar).second);
    int* idtmp = &(mela_event.pMothers.at(ipar).first);
    if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_tmp[ipar] = *idtmp;
    else MYIDUP_tmp[ipar] = 0;
    if (momTmp->T()>0.){
      p4[ipar][0] = momTmp->T()*GeV;
      p4[ipar][1] = momTmp->X()*GeV;
      p4[ipar][2] = momTmp->Y()*GeV;
      p4[ipar][3] = momTmp->Z()*GeV;
      MomStore[ipar] = (*momTmp);
    }
    else{
      p4[ipar][0] = -momTmp->T()*GeV;
      p4[ipar][1] = -momTmp->X()*GeV;
      p4[ipar][2] = -momTmp->Y()*GeV;
      p4[ipar][3] = -momTmp->Z()*GeV;
      MomStore[ipar] = -(*momTmp);
      MYIDUP_tmp[ipar] = -MYIDUP_tmp[ipar];
    }
  }
  for (unsigned int ipar=0; ipar<2; ipar++){
    if (ipar<mela_event.pAssociated.size()){
      TLorentzVector* momTmp = &(mela_event.pAssociated.at(ipar).second);
      int* idtmp = &(mela_event.pAssociated.at(ipar).first);
      if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_tmp[ipar+2] = *idtmp;
      else MYIDUP_tmp[ipar+2] = 0;
      p4[ipar+2][0] = momTmp->T()*GeV;
      p4[ipar+2][1] = momTmp->X()*GeV;
      p4[ipar+2][2] = momTmp->Y()*GeV;
      p4[ipar+2][3] = momTmp->Z()*GeV;
      MomStore[ipar+6] = (*momTmp); // i==(2, 3, 4) is (J1, J2, H), recorded as MomStore (I1, I2, 0, 0, 0, H, J1, J2)
    }
    else MYIDUP_tmp[ipar+2] = -9000; // No need to set p4, which is already 0 by initialization
  }
  for (unsigned int ipar=0; ipar<mela_event.pDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(mela_event.pDaughters.at(ipar).second);
    p4[4][0] += momTmp->T()*GeV;
    p4[4][1] += momTmp->X()*GeV;
    p4[4][2] += momTmp->Y()*GeV;
    p4[4][3] += momTmp->Z()*GeV;
    MomStore[5] = MomStore[5] + (*momTmp); // i==(2, 3, 4) is (J1, J2, H), recorded as MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }
  // Momenta for HJ
  for (unsigned int i = 0; i < 4; i++){
    if (i<3){ for (int j = 0; j < 4; j++) pOneJet[i][j] = p4[i][j]; } // p1 p2 J1
    else{ for (int j = 0; j < 4; j++) pOneJet[i][j] = p4[i+1][j]; } // H
  }
  if (verbosity >= TVar::DEBUG){ for (int i=0; i<5; i++) cout << "p["<<i<<"] (Px, Py, Pz, E):\t" << p4[i][1]/GeV << '\t' << p4[i][2]/GeV << '\t' << p4[i][3]/GeV << '\t' << p4[i][0]/GeV << endl; }

  double defaultRenScale = scale_.scale;
  double defaultFacScale = facscale_.facscale;
  //cout << "Default scales: " << defaultRenScale << '\t' << defaultFacScale << endl;
  int defaultNloop = nlooprun_.nlooprun;
  int defaultNflav = nflav_.nflav;
  string defaultPdflabel = pdlabel_.pdlabel;
  double renQ = InterpretScaleScheme(production, matrixElement, event_scales->renomalizationScheme, MomStore);
  //cout << "renQ: " << renQ << " x " << event_scales->ren_scale_factor << endl;
  double facQ = InterpretScaleScheme(production, matrixElement, event_scales->factorizationScheme, MomStore);
  //cout << "facQ: " << facQ << " x " << event_scales->fac_scale_factor << endl;
  SetAlphaS(renQ, facQ, event_scales->ren_scale_factor, event_scales->fac_scale_factor, 1, 5, "cteq6_l"); // Set AlphaS(|Q|/2, mynloop, mynflav, mypartonPDF)

  // NOTE ON CHANNEL HASHES:
  // THEY ONLY RETURN ISEL>=JSEL CASES. ISEL<JSEL NEEDS TO BE DONE MANUALLY.
  if (production == TVar::JH){ // Computation is already for all possible qqb/qg/qbg/gg, and incoming q, qb and g flavor have 1-1 correspondence to the outgoing jet flavor.
    __modhiggsj_MOD_evalamp_hj(pOneJet, MatElsq_tmp);
    for (int isel=-5; isel<=5; isel++){
      if (MYIDUP_tmp[0]!=0 && !((PDGHelpers::isAGluon(MYIDUP_tmp[0]) && isel==0) || MYIDUP_tmp[0]==isel)) continue;
      for (int jsel=-5; jsel<=5; jsel++){
        if (MYIDUP_tmp[1]!=0 && !((PDGHelpers::isAGluon(MYIDUP_tmp[1]) && jsel==0) || MYIDUP_tmp[1]==jsel)) continue;
        int rsel;
        if (isel!=0 && jsel!=0) rsel=0; // Covers qqb->Hg
        else if (isel==0) rsel=jsel; // Covers gg->Hg, gq->Hq, gqb->Hqb
        else rsel=isel; // Covers qg->Hq, qbg->Hqb
        if (MYIDUP_tmp[2]!=0 && !((PDGHelpers::isAGluon(MYIDUP_tmp[2]) && rsel==0) || MYIDUP_tmp[2]==rsel)) continue;
        MatElsq[jsel+5][isel+5] = MatElsq_tmp[jsel+5][isel+5]; // Assign only those that match gen. info, if present at all.
      }
    }
  }
  else if (production==TVar::JJGG){
    int ijsel[3][121];
    int nijchannels=77;
    __modhiggsjj_MOD_get_hjjchannelhash_nosplit(ijsel, &nijchannels);
    for (int ic=0; ic<nijchannels; ic++){
      // Emulate EvalWeighted_HJJ_test
      int isel = ijsel[0][ic];
      int jsel = ijsel[1][ic];
      int code = ijsel[2][ic];
      // Default assignments
      int rsel=isel;
      int ssel=jsel;
      if (
        (MYIDUP_tmp[0]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[0]) && isel==0) || MYIDUP_tmp[0]==isel))
        &&
        (MYIDUP_tmp[1]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[1]) && jsel==0) || MYIDUP_tmp[1]==jsel))
        ){ // Do it this way to be able to swap isel and jsel later

        if (isel==0 && jsel==0){ // gg->?
          if (code==2){ // gg->qqb
            // Only compute u-ub. The amplitude is multiplied by nf=5
            rsel=1;
            ssel=-1;
            double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
            if (
              (MYIDUP_tmp[2]==0 || (PDGHelpers::isAQuark(MYIDUP_tmp[2]) && MYIDUP_tmp[2]>0))
              &&
              (MYIDUP_tmp[3]==0 || (PDGHelpers::isAQuark(MYIDUP_tmp[3]) && MYIDUP_tmp[3]<0))
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
            if (
              (MYIDUP_tmp[2]==0 || (PDGHelpers::isAQuark(MYIDUP_tmp[2]) && MYIDUP_tmp[2]<0))
              &&
              (MYIDUP_tmp[3]==0 || (PDGHelpers::isAQuark(MYIDUP_tmp[3]) && MYIDUP_tmp[3]>0))
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
          }
          else{ // gg->gg
            // rsel=ssel=g already
            if (
              (MYIDUP_tmp[2]==0 || (PDGHelpers::isAGluon(MYIDUP_tmp[2]) && rsel==0))
              &&
              (MYIDUP_tmp[3]==0 || (PDGHelpers::isAGluon(MYIDUP_tmp[3]) && ssel==0))
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]; // Assign only those that match gen. info, if present at all.
            }
          }
        }
        else if (isel==0 || jsel==0){ // qg/qbg/gq/gqb->qg/qbg/gq/gqb
          double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
          if (
            (MYIDUP_tmp[2]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[2]) && rsel==0) || MYIDUP_tmp[2]==rsel))
            &&
            (MYIDUP_tmp[3]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[3]) && ssel==0) || MYIDUP_tmp[3]==ssel))
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
          if (
            (MYIDUP_tmp[2]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[2]) && ssel==0) || MYIDUP_tmp[2]==ssel))
            &&
            (MYIDUP_tmp[3]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[3]) && rsel==0) || MYIDUP_tmp[3]==rsel))
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
        }
        else if ((isel>0 && jsel<0) || (isel<0 && jsel>0)){ // qQb/qbQ->?
          if (code==1){ // qqb/qbq->gg
            rsel=0; ssel=0;
            if (
              (MYIDUP_tmp[2]==0 || PDGHelpers::isAGluon(MYIDUP_tmp[2]))
              &&
              (MYIDUP_tmp[3]==0 || PDGHelpers::isAGluon(MYIDUP_tmp[3]))
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]; // Assign only those that match gen. info, if present at all.
            }
          }
          else if (code==2){ // qQb/qbQ->qQb/qbQ
            double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
          }
          else{ // qqb->QQb
            if (abs(isel)!=1){ rsel=1; ssel=-1; } // Make sure rsel, ssel are not of same flavor as isel, jsel
            else{ rsel=2; ssel=-2; }
            // The amplitude is aready multiplied by nf-1, so no need to calculate everything (nf-1) times.
            double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
          }
        }
        else{
          double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0 && rsel!=ssel) avgfac=0.5;
          if (
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
          if (
            rsel!=ssel
            &&
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
        }
      } // End unswapped isel>=jsel cases
      if (isel==jsel) continue;
      isel = ijsel[1][ic];
      jsel = ijsel[0][ic];
      // Reset to default assignments
      rsel=isel;
      ssel=jsel;
      if (
        (MYIDUP_tmp[0]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[0]) && isel==0) || MYIDUP_tmp[0]==isel))
        &&
        (MYIDUP_tmp[1]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[1]) && jsel==0) || MYIDUP_tmp[1]==jsel))
        ){
        // isel==jsel==0 is already eliminated by isel!=jsel condition
        if (isel==0 || jsel==0){ // qg/qbg/gq/gqb->qg/qbg/gq/gqb
          double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
          if (
            (MYIDUP_tmp[2]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[2]) && rsel==0) || MYIDUP_tmp[2]==rsel))
            &&
            (MYIDUP_tmp[3]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[3]) && ssel==0) || MYIDUP_tmp[3]==ssel))
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
          if (
            (MYIDUP_tmp[2]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[2]) && ssel==0) || MYIDUP_tmp[2]==ssel))
            &&
            (MYIDUP_tmp[3]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[3]) && rsel==0) || MYIDUP_tmp[3]==rsel))
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
        }
        else if ((isel>0 && jsel<0) || (isel<0 && jsel>0)){ // qQb/qbQ->?
          if (code==1){ // qqb/qbq->gg
            rsel=0; ssel=0;
            if (
              (MYIDUP_tmp[2]==0 || PDGHelpers::isAGluon(MYIDUP_tmp[2]))
              &&
              (MYIDUP_tmp[3]==0 || PDGHelpers::isAGluon(MYIDUP_tmp[3]))
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]; // Assign only those that match gen. info, if present at all.
            }
          }
          else if (code==2){ // qQb/qbQ->qQb/qbQ
            double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
          }
          else{ // qqb->QQb
            if (abs(isel)!=1){ rsel=1; ssel=-1; } // Make sure rsel, ssel are not of same flavor as isel, jsel
            else{ rsel=2; ssel=-2; }
            // The amplitude is aready multiplied by nf-1, so no need to calculate everything (nf-1) times.
            double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0) avgfac=0.5;
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
            if (
              (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
              &&
              (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
              ){
              __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
              MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
            }
          }
        }
        else{
          double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0 && rsel!=ssel) avgfac=0.5;
          if (
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
          if (
            rsel!=ssel
            &&
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
            ){
            __modhiggsjj_MOD_evalamp_sbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
        }
      } // End swapped isel<jsel cases
    } // End loop over ic<nijchannels
  } // End production==TVar::JJGG
  else if (production==TVar::JJVBF){
    int ijsel[3][121];
    int nijchannels=68;
    __modhiggsjj_MOD_get_vbfchannelhash_nosplit(ijsel, &nijchannels);
    for (int ic=0; ic<nijchannels; ic++){
      // Emulate EvalWeighted_HJJ_test
      int isel = ijsel[0][ic];
      int jsel = ijsel[1][ic];
      int code = ijsel[2][ic];
      // Default assignments
      int rsel=isel;
      int ssel=jsel;
      if (
        (MYIDUP_tmp[0]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[0]) && isel==0) || MYIDUP_tmp[0]==isel))
        &&
        (MYIDUP_tmp[1]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[1]) && jsel==0) || MYIDUP_tmp[1]==jsel))
        ){ // Do it this way to be able to swap isel and jsel later
        if (code==1){ // Only ZZ->H possible
          // rsel=isel and ssel=jsel already
          double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0 && rsel!=ssel) avgfac=0.5;
          if (
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
            ){
            __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
          if (
            rsel!=ssel
            &&
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
            ){
            __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
        }
        else{ // code==0 means WW->H is also possible with no interference to ZZ->H, and code==2 means the same except interference to ZZ->H could occur for some outgoing quark flavors.
          vector<int> possible_rsel;
          vector<int> possible_ssel;
          if (PDGHelpers::isUpTypeQuark(isel)){ possible_rsel.push_back(1); possible_rsel.push_back(3); possible_rsel.push_back(5); }
          else if (PDGHelpers::isDownTypeQuark(isel)){ possible_rsel.push_back(2); possible_rsel.push_back(4); }
          if (PDGHelpers::isUpTypeQuark(jsel)){ possible_ssel.push_back(1); possible_ssel.push_back(3); possible_ssel.push_back(5); }
          else if (PDGHelpers::isDownTypeQuark(jsel)){ possible_ssel.push_back(2); possible_ssel.push_back(4); }
          for (unsigned int ix=0; ix<possible_rsel.size(); ix++){
            for (unsigned int iy=0; iy<possible_ssel.size(); iy++){
              rsel=possible_rsel.at(ix)*TMath::Sign(1, isel);
              ssel=possible_ssel.at(ix)*TMath::Sign(1, jsel);
              double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0 && rsel!=ssel) avgfac=0.5;
              if (
                (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
                &&
                (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
                ){
                __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
                MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
              }
              if (
                rsel!=ssel
                &&
                (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
                &&
                (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
                ){
                __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
                MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
              }
            }
          }
        }
      } // End unswapped isel>=jsel cases
      if (isel==jsel) continue;
      isel = ijsel[1][ic];
      jsel = ijsel[0][ic];
      // Reset to default assignments
      rsel=isel;
      ssel=jsel;
      if (
        (MYIDUP_tmp[0]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[0]) && isel==0) || MYIDUP_tmp[0]==isel))
        &&
        (MYIDUP_tmp[1]==0 || ((PDGHelpers::isAGluon(MYIDUP_tmp[1]) && jsel==0) || MYIDUP_tmp[1]==jsel))
        ){
        // isel==jsel==0 is already eliminated by isel!=jsel condition
        if (code==1){ // Only ZZ->H possible
          // rsel=isel and ssel=jsel already
          double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0 && rsel!=ssel) avgfac=0.5;
          if (
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
            ){
            __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
          if (
            rsel!=ssel
            &&
            (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
            &&
            (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
            ){
            __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
            MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
          }
        }
        else{ // code==0 means WW->H is also possible with no interference to ZZ->H, and code==2 means the same except interference to ZZ->H could occur for some outgoing quark flavors.
          vector<int> possible_rsel;
          vector<int> possible_ssel;
          if (PDGHelpers::isUpTypeQuark(isel)){ possible_rsel.push_back(1); possible_rsel.push_back(3); possible_rsel.push_back(5); }
          else if (PDGHelpers::isDownTypeQuark(isel)){ possible_rsel.push_back(2); possible_rsel.push_back(4); }
          if (PDGHelpers::isUpTypeQuark(jsel)){ possible_ssel.push_back(1); possible_ssel.push_back(3); possible_ssel.push_back(5); }
          else if (PDGHelpers::isDownTypeQuark(jsel)){ possible_ssel.push_back(2); possible_ssel.push_back(4); }
          for (unsigned int ix=0; ix<possible_rsel.size(); ix++){
            for (unsigned int iy=0; iy<possible_ssel.size(); iy++){
              rsel=possible_rsel.at(ix)*TMath::Sign(1, isel);
              ssel=possible_ssel.at(ix)*TMath::Sign(1, jsel);
              double avgfac=1.; if (MYIDUP_tmp[2]==0 && MYIDUP_tmp[3]==0 && rsel!=ssel) avgfac=0.5;
              if (
                (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==rsel)
                &&
                (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==ssel)
                ){
                __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &rsel, &ssel, MatElsq_tmp);
                MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
              }
              if (
                rsel!=ssel
                &&
                (MYIDUP_tmp[2]==0 || MYIDUP_tmp[2]==ssel)
                &&
                (MYIDUP_tmp[3]==0 || MYIDUP_tmp[3]==rsel)
                ){
                __modhiggsjj_MOD_evalamp_wbfh_unsymm_sa_select_exact(p4, &isel, &jsel, &ssel, &rsel, MatElsq_tmp);
                MatElsq[jsel+5][isel+5] += MatElsq_tmp[jsel+5][isel+5]*avgfac; // Assign only those that match gen. info, if present at all.
              }
            }
          }
        }
      } // End swapped isel<jsel cases
    } // End loop over ic<nijchannels
  } // End production==TVar::JJVBF

  //    FOTRAN convention    -5    -4   -3   -2   -1    0   1   2   3  4  5
  //     parton flavor      bbar  cbar  sbar ubar dbar  g   d   u   s  c  b
  //      C++ convention     0      1    2    3    4    5   6   7   8  9  10
  for (int ii = 0; ii < nmsq; ii++){ for (int jj = 0; jj < nmsq; jj++){ if (verbosity >= TVar::DEBUG) cout<< "MatElsq: " << ii-5 << " " << jj-5 << " " << MatElsq[jj][ii] << endl; } }

  sum_msqjk = SumMEPDF(MomStore[0], MomStore[1], MatElsq, RcdME, EBEAM, verbosity);

  //cout << "Before reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  SetAlphaS(defaultRenScale, defaultFacScale, 1., 1., defaultNloop, defaultNflav, defaultPdflabel); // Protection for other probabilities
  //cout << "Default scale reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  return sum_msqjk;
}

double TUtil::VHiggsMatEl(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, MelaIO* RcdME,
  double EBEAM,
  bool includeHiggsDecay,
  TVar::VerbosityLevel verbosity
  ){
  const double GeV=1./100.; // JHUGen mom. scale factor
  double sum_msqjk = 0;
  // by default assume only gg productions
  // FOTRAN convention -5    -4   -3  -2    -1  0 1 2 3 4 5
  //     parton flavor bbar cbar sbar ubar dbar g d u s c b
  // C++ convention     0     1   2    3    4   5 6 7 8 9 10
  //2-D matrix is reversed in fortran
  // msq[ parton2 ] [ parton1 ]
  //      flavor_msq[jj][ii] = fx1[ii]*fx2[jj]*msq[jj][ii];
  double MatElsq[nmsq][nmsq]={ { 0 } };

  if (matrixElement!=TVar::JHUGen){ cerr << "TUtil::VHiggsMatEl: Non-JHUGen MEs are not supported" << endl; return sum_msqjk; }
  if (!(production == TVar::Lep_ZH || production == TVar::Lep_WH || production == TVar::Had_ZH || production == TVar::Had_WH || production == TVar::GammaH)){ cerr << "TUtil::VHiggsMatEl: Production is not supported!" << endl; return sum_msqjk; }

  int nRequested_AssociatedJets=0;
  int nRequested_AssociatedLeptons=0;
  int nRequested_AssociatedPhotons=0;
  int AssociationVCompatibility=0;
  int partIncCode=TVar::kNoAssociated; // Just to avoid warnings
  if (production == TVar::Had_ZH || production == TVar::Had_WH){ // Only use associated partons
    partIncCode=TVar::kUseAssociated_Jets;
    nRequested_AssociatedJets=2;
  }
  else if (production == TVar::Lep_ZH || production == TVar::Lep_WH){ // Only use associated leptons(+)neutrinos
    partIncCode=TVar::kUseAssociated_Leptons;
    nRequested_AssociatedLeptons=2;
  }
  else if (production == TVar::GammaH){ // Only use associated photon
    partIncCode=TVar::kUseAssociated_Photons;
    nRequested_AssociatedPhotons=1;
  }
  if (production==TVar::Lep_WH || production==TVar::Had_WH) AssociationVCompatibility=24;
  else if (production==TVar::Lep_ZH || production==TVar::Had_ZH) AssociationVCompatibility=23;
  else if (production==TVar::GammaH) AssociationVCompatibility=22;
  simple_event_record mela_event;
  mela_event.AssociationCode=partIncCode;
  mela_event.AssociationVCompatibility=AssociationVCompatibility;
  mela_event.nRequested_AssociatedJets=nRequested_AssociatedJets;
  mela_event.nRequested_AssociatedLeptons=nRequested_AssociatedLeptons;
  mela_event.nRequested_AssociatedPhotons=nRequested_AssociatedPhotons;
  GetBoostedParticleVectors(
    RcdME->melaCand,
    mela_event
    );
  if ((mela_event.pAssociated.size()<2 && production!=TVar::GammaH) || (mela_event.pAssociated.size()<1 && production==TVar::GammaH)){
    if (verbosity>=TVar::INFO) cerr << "TUtil::VHiggsMatEl: Number of associated particles is not supported!" << endl;
    return sum_msqjk;
  }

  int MYIDUP_prod[4]={ 0 }; // "Incoming" partons 1, 2, "outgoing" partons 3, 4
  int MYIDUP_dec[2]={ -9000, -9000 }; // "Outgoing" partons 1, 2 from the Higgs (->bb)
  double p4[9][4] ={ { 0 } };
  double helicities[9] ={ 0 };
  int vh_ids[9] ={ 0 };
  TLorentzVector MomStore[mxpart];
  for (int i = 0; i < mxpart; i++) MomStore[i].SetXYZT(0, 0, 0, 0);

  // p4(0:8,i) = (E(i),px(i),py(i),pz(i))
  // i=0,1: q1, qb2 (outgoing convention)
  // i=2,3: V*, V
  // i=4: H
  // i=5,6: f, fb from V
  // i=7,8: b, bb from H
  for (int ipar=0; ipar<2; ipar++){
    TLorentzVector* momTmp = &(mela_event.pMothers.at(ipar).second);
    int* idtmp = &(mela_event.pMothers.at(ipar).first);
    if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_prod[ipar] = *idtmp;
    else MYIDUP_prod[ipar] = 0;
    if (momTmp->T()>0.){
      p4[ipar][0] = momTmp->T()*GeV;
      p4[ipar][1] = momTmp->X()*GeV;
      p4[ipar][2] = momTmp->Y()*GeV;
      p4[ipar][3] = momTmp->Z()*GeV;
      MomStore[ipar] = (*momTmp);
    }
    else{
      p4[ipar][0] = -momTmp->T()*GeV;
      p4[ipar][1] = -momTmp->X()*GeV;
      p4[ipar][2] = -momTmp->Y()*GeV;
      p4[ipar][3] = -momTmp->Z()*GeV;
      MomStore[ipar] = -(*momTmp);
      MYIDUP_prod[ipar] = -MYIDUP_prod[ipar];
    }
  }
  // Associated particles
  for (int ipar=0; ipar<(production!=TVar::GammaH ? 2 : 1); ipar++){
    TLorentzVector* momTmp = &(mela_event.pAssociated.at(ipar).second);
    int* idtmp = &(mela_event.pAssociated.at(ipar).first);
    if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_prod[ipar+2] = *idtmp;
    else MYIDUP_prod[ipar+2] = 0;
    p4[ipar+5][0] = momTmp->T()*GeV;
    p4[ipar+5][1] = momTmp->X()*GeV;
    p4[ipar+5][2] = momTmp->Y()*GeV;
    p4[ipar+5][3] = momTmp->Z()*GeV;
    MomStore[ipar+6] = (*momTmp);
  }
  if (production==TVar::GammaH) MYIDUP_prod[3]=-9000;

  if (PDGHelpers::isAGluon(MYIDUP_prod[0]) || PDGHelpers::isAGluon(MYIDUP_prod[1])){ if (verbosity>=TVar::INFO) cerr << "TUtil::VHiggsMatEl: Initial state gluons are not permitted!" << endl; return sum_msqjk; }
  if (PDGHelpers::isAGluon(MYIDUP_prod[2]) || PDGHelpers::isAGluon(MYIDUP_prod[3])){ if (verbosity>=TVar::INFO) cerr << "TUtil::VHiggsMatEl: Final state gluons are not permitted!" << endl; return sum_msqjk; }

  // Decay V/f ids
  for (int iv=0; iv<2; iv++){
    int idtmp = mela_event.intermediateVid.at(iv);
    if (!PDGHelpers::isAnUnknownJet(idtmp)) MYIDUP_dec[iv] = idtmp;
    else MYIDUP_dec[iv] = 0;
  }
  // Decay daughters
  for (unsigned int ipar=0; ipar<mela_event.pDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(mela_event.pDaughters.at(ipar).second);
    if (mela_event.pDaughters.size()==1){
      p4[ipar+7][0] = momTmp->T()*GeV;
      p4[ipar+7][1] = momTmp->X()*GeV;
      p4[ipar+7][2] = momTmp->Y()*GeV;
      p4[ipar+7][3] = momTmp->Z()*GeV;
      MomStore[5] = (*momTmp); // 5
    }
    else if (mela_event.pDaughters.size()==2){
      p4[ipar+7][0] = momTmp->T()*GeV;
      p4[ipar+7][1] = momTmp->X()*GeV;
      p4[ipar+7][2] = momTmp->Y()*GeV;
      p4[ipar+7][3] = momTmp->Z()*GeV;
      MomStore[2*ipar+2] = (*momTmp); // 2,4
      if (PDGHelpers::isAQuark(mela_event.pDaughters.at(ipar).first)) MYIDUP_dec[ipar]=mela_event.pDaughters.at(ipar).first;
    }
    else if (mela_event.pDaughters.size()==3){
      if (ipar<2){
        p4[7][0] += momTmp->T()*GeV;
        p4[7][1] += momTmp->X()*GeV;
        p4[7][2] += momTmp->Y()*GeV;
        p4[7][3] += momTmp->Z()*GeV;
      }
      else{
        p4[8][0] = momTmp->T()*GeV;
        p4[8][1] = momTmp->X()*GeV;
        p4[8][2] = momTmp->Y()*GeV;
        p4[8][3] = momTmp->Z()*GeV;
      }
      MomStore[ipar+2] = (*momTmp); // 2,3,4
    }
    else if (mela_event.pDaughters.size()==4){
      if (ipar<2){
        p4[7][0] += momTmp->T()*GeV;
        p4[7][1] += momTmp->X()*GeV;
        p4[7][2] += momTmp->Y()*GeV;
        p4[7][3] += momTmp->Z()*GeV;
      }
      else{
        p4[8][0] += momTmp->T()*GeV;
        p4[8][1] += momTmp->X()*GeV;
        p4[8][2] += momTmp->Y()*GeV;
        p4[8][3] += momTmp->Z()*GeV;
      }
      MomStore[ipar+2] = (*momTmp); // 2,3,4,5
    }
    else{ // Should never happen
      p4[7][0] += momTmp->T()*GeV;
      p4[7][1] += momTmp->X()*GeV;
      p4[7][2] += momTmp->Y()*GeV;
      p4[7][3] += momTmp->Z()*GeV;
      MomStore[5] = MomStore[5] + (*momTmp);
    }
  }
  for (int ix=0; ix<4; ix++){
    p4[3][ix] = p4[5][ix] + p4[6][ix];
    p4[4][ix] = p4[7][ix] + p4[8][ix];
    p4[2][ix] = p4[3][ix] + p4[4][ix];
  }

  vh_ids[4] = 25;
  if (production==TVar::Lep_ZH || production==TVar::Had_ZH || production==TVar::GammaH) vh_ids[2] = 23;
  else if (production==TVar::Lep_WH || production==TVar::Had_WH) vh_ids[2] = 24; // To be changed later
  if (production!=TVar::GammaH) vh_ids[3] = vh_ids[2]; // To be changed later for WH
  else vh_ids[3] = 22;

  // H->ffb decay is turned off, so no need to loop over helicities[7]=helicities[8]=+-1
  vh_ids[7] = 5; helicities[7] = 1;
  vh_ids[8] = -5; helicities[8] = 1;
  int HDKon = 0;
  if (includeHiggsDecay && MYIDUP_dec[0]!=-9000 && MYIDUP_dec[1]!=-9000 && MYIDUP_dec[0]==-MYIDUP_dec[1]){
    HDKon=1;
    __modjhugenmela_MOD_sethdk(&HDKon);
  }
  else if (verbosity>=TVar::INFO && includeHiggsDecay) cerr << "TUtil::VHiggsMatEl: includeHiggsDecay=true is not supported for the present decay mode." << endl;

  if (verbosity >= TVar::DEBUG){
    for (int i=0; i<9; i++) cout << "p4[0] = "  << p4[i][0] << ", " <<  p4[i][1] << ", "  <<  p4[i][2] << ", "  <<  p4[i][3] << "\n";
    for (int i=0; i<9; i++) cout << "id(" << i << ") = "  << vh_ids[i] << endl;
  }

  double defaultRenScale = scale_.scale;
  double defaultFacScale = facscale_.facscale;
  //cout << "Default scales: " << defaultRenScale << '\t' << defaultFacScale << endl;
  int defaultNloop = nlooprun_.nlooprun;
  int defaultNflav = nflav_.nflav;
  string defaultPdflabel = pdlabel_.pdlabel;
  double renQ = InterpretScaleScheme(production, matrixElement, event_scales->renomalizationScheme, MomStore);
  //cout << "renQ: " << renQ << " x " << event_scales->ren_scale_factor << endl;
  double facQ = InterpretScaleScheme(production, matrixElement, event_scales->factorizationScheme, MomStore);
  //cout << "facQ: " << facQ << " x " << event_scales->fac_scale_factor << endl;
  SetAlphaS(renQ, facQ, event_scales->ren_scale_factor, event_scales->fac_scale_factor, 1, 5, "cteq6_l"); // Set AlphaS(|Q|/2, mynloop, mynflav, mypartonPDF)

  const double allowed_helicities[2] = { -1, 1 }; // L,R
  for (int h01 = 0; h01 < 2; h01++){
    helicities[0] = allowed_helicities[h01];
    helicities[1] = -helicities[0];
    for (int h56 = 0; h56 < 2; h56++){
      helicities[5] = allowed_helicities[h56];
      helicities[6] = -helicities[5];
      for (int incoming1 = -nf; incoming1 <= nf; incoming1++){
        if (incoming1==0) continue;
        if (production==TVar::Lep_ZH || production==TVar::Had_ZH || production==TVar::GammaH){
          vh_ids[0] = incoming1;
          vh_ids[1] = -incoming1;
          if (
            (MYIDUP_prod[0]!=0 && MYIDUP_prod[0]!=vh_ids[0])
            ||
            (MYIDUP_prod[1]!=0 && MYIDUP_prod[1]!=vh_ids[1])
            ) continue;

          if (production==TVar::Had_ZH){
            for (int outgoing1=-nf; outgoing1<=nf; outgoing1++){
              vh_ids[5] = outgoing1;
              vh_ids[6] = -outgoing1;
              if (
                (MYIDUP_prod[2]!=0 && MYIDUP_prod[2]!=vh_ids[5])
                ||
                (MYIDUP_prod[3]!=0 && MYIDUP_prod[3]!=vh_ids[6])
                ) continue;

              if (HDKon==0){
                double msq=0;
                __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
              }
              else{
                for (int h78=0; h78<2; h78++){
                  helicities[7]=allowed_helicities[h78];
                  helicities[8]=allowed_helicities[h78];
                  if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0]) || !PDGHelpers::isAnUnknownJet(MYIDUP_dec[1])){
                    if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0])){
                      vh_ids[7]=MYIDUP_dec[0];
                      vh_ids[8]=-MYIDUP_dec[0];
                    }
                    else{
                      vh_ids[7]=-MYIDUP_dec[1];
                      vh_ids[8]=MYIDUP_dec[1];
                    }
                    double msq=0;
                    __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                    MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                  }
                  else{
                    for (int hquark=-5; hquark<=5; hquark++){
                      if (hquark==0) continue;
                      vh_ids[7]=-hquark;
                      vh_ids[8]=hquark;
                      double msq=0;
                      __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                      MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                    }
                  }
                }
              }

            } // End loop over outgoing1=-outgoing2
          }
          else{
            vh_ids[5] = MYIDUP_prod[2];
            vh_ids[6] = MYIDUP_prod[3];

            if (HDKon==0){
              double msq=0;
              __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
              MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
            }
            else{
              for (int h78=0; h78<2; h78++){
                helicities[7]=allowed_helicities[h78];
                helicities[8]=allowed_helicities[h78];
                if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0]) || !PDGHelpers::isAnUnknownJet(MYIDUP_dec[1])){
                  if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0])){
                    vh_ids[7]=MYIDUP_dec[0];
                    vh_ids[8]=-MYIDUP_dec[0];
                  }
                  else{
                    vh_ids[7]=-MYIDUP_dec[1];
                    vh_ids[8]=MYIDUP_dec[1];
                  }
                  double msq=0;
                  __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                  MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                }
                else{
                  for (int hquark=-5; hquark<=5; hquark++){
                    if (hquark==0) continue;
                    vh_ids[7]=-hquark;
                    vh_ids[8]=hquark;
                    double msq=0;
                    __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                    MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                  }
                }
              }
            }

          } // End check on Had vs Lep
        } // End ZH case
        else if (production==TVar::Lep_WH || production==TVar::Had_WH){
          vh_ids[0] = incoming1;
          if (MYIDUP_prod[0]!=0 && MYIDUP_prod[0]!=vh_ids[0]) continue;

          for (int incoming2 = -nf; incoming2 < 0; incoming2++){
            if (abs(incoming2)==abs(incoming1) || TMath::Sign(1, incoming1)==TMath::Sign(1, incoming2)) continue;

            vh_ids[1] = incoming2;
            if (MYIDUP_prod[1]!=0 && MYIDUP_prod[1]!=vh_ids[1]) continue;

            if (production==TVar::Had_WH){
              for (int outgoing1=-nf; outgoing1<=nf; outgoing1++){
                for (int outgoing2=-nf; outgoing2<=nf; outgoing2++){
                  if (abs(outgoing2)==abs(outgoing1) || TMath::Sign(1, outgoing1)==TMath::Sign(1, outgoing2)) continue;

                  // Determine whether the decay is from a W+ or a W-
                  if (
                    (PDGHelpers::isUpTypeQuark(outgoing1) && outgoing1>0)
                    ||
                    (PDGHelpers::isUpTypeQuark(outgoing2) && outgoing2>0)
                    ) vh_ids[2]=24;
                  else vh_ids[2]=-24;
                  vh_ids[3] = vh_ids[2];

                  vh_ids[5] = outgoing1;
                  vh_ids[6] = outgoing2;
                  if (
                    (MYIDUP_prod[2]!=0 && MYIDUP_prod[2]!=vh_ids[5])
                    ||
                    (MYIDUP_prod[3]!=0 && MYIDUP_prod[3]!=vh_ids[6])
                    ) continue;

                  if (HDKon==0){
                    double msq=0;
                    __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                    MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                  }
                  else{
                    for (int h78=0; h78<2; h78++){
                      helicities[7]=allowed_helicities[h78];
                      helicities[8]=allowed_helicities[h78];
                      if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0]) || !PDGHelpers::isAnUnknownJet(MYIDUP_dec[1])){
                        if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0])){
                          vh_ids[7]=MYIDUP_dec[0];
                          vh_ids[8]=-MYIDUP_dec[0];
                        }
                        else{
                          vh_ids[7]=-MYIDUP_dec[1];
                          vh_ids[8]=MYIDUP_dec[1];
                        }
                        double msq=0;
                        __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                        MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                      }
                      else{
                        for (int hquark=-5; hquark<=5; hquark++){
                          if (hquark==0) continue;
                          vh_ids[7]=-hquark;
                          vh_ids[8]=hquark;
                          double msq=0;
                          __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                          MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                        }
                      }
                    }
                  }

                } // End loop over outgoing2
              } // End loop over outgoing1
            }
            else{
              // Determine whether the decay is from a W+ or a W-
              if (
                (PDGHelpers::isANeutrino(MYIDUP_prod[2]) && MYIDUP_prod[2]>0)
                ||
                (PDGHelpers::isANeutrino(MYIDUP_prod[3]) && MYIDUP_prod[3]>0)
                ) vh_ids[2]=24;
              else vh_ids[2]=-24;
              vh_ids[3] = vh_ids[2];

              vh_ids[5] = MYIDUP_prod[2];
              vh_ids[6] = MYIDUP_prod[3];

              if (HDKon==0){
                double msq=0;
                __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
              }
              else{
                for (int h78=0; h78<2; h78++){
                  helicities[7]=allowed_helicities[h78];
                  helicities[8]=allowed_helicities[h78];
                  if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0]) || !PDGHelpers::isAnUnknownJet(MYIDUP_dec[1])){
                    if (!PDGHelpers::isAnUnknownJet(MYIDUP_dec[0])){
                      vh_ids[7]=MYIDUP_dec[0];
                      vh_ids[8]=-MYIDUP_dec[0];
                    }
                    else{
                      vh_ids[7]=-MYIDUP_dec[1];
                      vh_ids[8]=MYIDUP_dec[1];
                    }
                    double msq=0;
                    __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                    MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                  }
                  else{
                    for (int hquark=-5; hquark<=5; hquark++){
                      if (hquark==0) continue;
                      vh_ids[7]=-hquark;
                      vh_ids[8]=hquark;
                      double msq=0;
                      __modvhiggs_MOD_evalamp_vhiggs(vh_ids, helicities, p4, &msq);
                      MatElsq[vh_ids[0]+5][vh_ids[1]+5] += msq * 0.25; // Average over initial states with helicities +-1 only
                    }
                  }
                }
              }

            } // End check on Had vs Lep
          } // End loop over incoming2
        } // End WH case
      } // End loop over incoming1
    } // End loop over h56
  } // End loop over h01

  sum_msqjk = SumMEPDF(MomStore[0], MomStore[1], MatElsq, RcdME, EBEAM, verbosity);

  // Turn H_DK off
  if (HDKon!=0){
    HDKon=0;
    __modjhugenmela_MOD_sethdk(&HDKon);
  }

  //cout << "Before reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  SetAlphaS(defaultRenScale, defaultFacScale, 1., 1., defaultNloop, defaultNflav, defaultPdflabel); // Protection for other probabilities
  //cout << "Default scale reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  
  return sum_msqjk;
}


// ttH
double TUtil::TTHiggsMatEl(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, MelaIO* RcdME,
  double EBEAM,
  int topDecay, int topProcess,
  TVar::VerbosityLevel verbosity
  ){
  const double GeV=1./100.; // JHUGen mom. scale factor
  double sum_msqjk = 0;
  double MatElsq[nmsq][nmsq]={ { 0 } };
  double MatElsq_tmp[nmsq][nmsq]={ { 0 } };

  if (matrixElement!=TVar::JHUGen){ cerr << "TUtil::TTHiggsMatEl: Non-JHUGen MEs are not supported." << endl; return sum_msqjk; }
  if (production!=TVar::ttH){ cerr << "TUtil::TTHiggsMatEl: Only ttH is supported." << endl; return sum_msqjk; }

  int partIncCode;
  int nRequested_Tops=1;
  int nRequested_Antitops=1;
  if (topDecay>0) partIncCode=TVar::kUseAssociated_UnstableTops; // Look for unstable tops
  else partIncCode=TVar::kUseAssociated_StableTops; // Look for unstable tops

  simple_event_record mela_event;
  mela_event.AssociationCode=partIncCode;
  mela_event.nRequested_Tops=nRequested_Tops;
  mela_event.nRequested_Antitops=nRequested_Antitops;
  GetBoostedParticleVectors(
    RcdME->melaCand,
    mela_event
    );


  if (topDecay>0 && mela_event.pTopDaughters.size()<1 && mela_event.pAntitopDaughters.size()<1){
    cerr
      << "TUtil::TTHiggsMatEl: Number of set of top daughters (" << mela_event.pTopDaughters.size() << ")"
      << "and number of set of antitop daughters (" << mela_event.pAntitopDaughters.size() << ")"
      <<" in ttH process is not 1!" << endl;
    return sum_msqjk;
  }
  else if (topDecay>0 && mela_event.pTopDaughters.at(0).size()!=3 && mela_event.pAntitopDaughters.at(0).size()!=3){
    cerr
      << "TUtil::TTHiggsMatEl: Number of top daughters (" << mela_event.pTopDaughters.at(0).size() << ")"
      << "and number of antitop daughters (" << mela_event.pAntitopDaughters.at(0).size() << ")"
      <<" in ttH process is not 3!" << endl;
    return sum_msqjk;
  }
  else if (topDecay==0 && mela_event.pStableTops.size()<1 && mela_event.pStableAntitops.size()<1){
    cerr
      << "TUtil::TTHiggsMatEl: Number of stable tops (" << mela_event.pStableTops.size() << ")"
      << "and number of sstable antitops (" << mela_event.pStableAntitops.size() << ")"
      <<" in ttH process is not 1!" << endl;
    return sum_msqjk;
  }

  SimpleParticleCollection_t topDaughters;
  SimpleParticleCollection_t antitopDaughters;
  bool isUnknown[2]; isUnknown[0]=true; isUnknown[1]=true;

  if (topDecay>0){
    // Daughters are assumed to have been ordered as b, Wf, Wfb already.
    for (unsigned int itd=0; itd<mela_event.pTopDaughters.at(0).size(); itd++) topDaughters.push_back(mela_event.pTopDaughters.at(0).at(itd));
    for (unsigned int itd=0; itd<mela_event.pAntitopDaughters.at(0).size(); itd++) antitopDaughters.push_back(mela_event.pAntitopDaughters.at(0).at(itd));
  }
  else{
    for (unsigned int itop=0; itop<mela_event.pStableTops.size(); itop++) topDaughters.push_back(mela_event.pStableTops.at(itop));
    for (unsigned int itop=0; itop<mela_event.pStableAntitops.size(); itop++) antitopDaughters.push_back(mela_event.pStableAntitops.at(itop));
  }
  // Check if either top is definitely identified
  for (unsigned int itd=0; itd<topDaughters.size(); itd++){ if (topDaughters.at(itd).first!=0){ isUnknown[0]=false; break; } }
  for (unsigned int itd=0; itd<antitopDaughters.size(); itd++){ if (antitopDaughters.at(itd).first!=0){ isUnknown[1]=false; break; } }

  // Start assigning the momenta
  // 0,1: p1 p2
  // 2-4: H,tb,t
  // 5-8: bb,W-,f,fb
  // 9-12: b,W+,fb,f
  double p4[13][4]={ { 0 } };
  double MYIDUP_prod[2]={ 0 };
  TLorentzVector MomStore[mxpart];
  for (int i = 0; i < mxpart; i++) MomStore[i].SetXYZT(0, 0, 0, 0);
  for (int ipar=0; ipar<2; ipar++){
    TLorentzVector* momTmp = &(mela_event.pMothers.at(ipar).second);
    int* idtmp = &(mela_event.pMothers.at(ipar).first);
    if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_prod[ipar] = *idtmp;
    else MYIDUP_prod[ipar] = 0;
    if (momTmp->T()>0.){
      p4[ipar][0] = -momTmp->T()*GeV;
      p4[ipar][1] = -momTmp->X()*GeV;
      p4[ipar][2] = -momTmp->Y()*GeV;
      p4[ipar][3] = -momTmp->Z()*GeV;
      MomStore[ipar] = (*momTmp);
    }
    else{
      p4[ipar][0] = momTmp->T()*GeV;
      p4[ipar][1] = momTmp->X()*GeV;
      p4[ipar][2] = momTmp->Y()*GeV;
      p4[ipar][3] = momTmp->Z()*GeV;
      MomStore[ipar] = -(*momTmp);
      MYIDUP_prod[ipar] = -MYIDUP_prod[ipar];
    }
  }

  // Assign top momenta
  for (unsigned int ipar=0; ipar<topDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(topDaughters.at(ipar).second);
    if (topDaughters.size()==1){
      p4[4][0] = momTmp->T()*GeV;
      p4[4][1] = momTmp->X()*GeV;
      p4[4][2] = momTmp->Y()*GeV;
      p4[4][3] = momTmp->Z()*GeV;
    }
    // size==3
    else if (ipar==0){ // b
      p4[9][0] = momTmp->T()*GeV;
      p4[9][1] = momTmp->X()*GeV;
      p4[9][2] = momTmp->Y()*GeV;
      p4[9][3] = momTmp->Z()*GeV;
    }
    else if (ipar==2){ // Wfb
      p4[11][0] = momTmp->T()*GeV;
      p4[11][1] = momTmp->X()*GeV;
      p4[11][2] = momTmp->Y()*GeV;
      p4[11][3] = momTmp->Z()*GeV;
      p4[10][0] += p4[11][0];
      p4[10][1] += p4[11][1];
      p4[10][2] += p4[11][2];
      p4[10][3] += p4[11][3];
    }
    else/* if (ipar==1)*/{ // Wf
      p4[12][0] = momTmp->T()*GeV;
      p4[12][1] = momTmp->X()*GeV;
      p4[12][2] = momTmp->Y()*GeV;
      p4[12][3] = momTmp->Z()*GeV;
      p4[10][0] += p4[12][0];
      p4[10][1] += p4[12][1];
      p4[10][2] += p4[12][2];
      p4[10][3] += p4[12][3];
    }
    MomStore[6] = MomStore[6] + (*momTmp); // MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }
  if (topDaughters.size()!=1){ for (int ix=0; ix<4; ix++){ for (int ip=9; ip<=10; ip++) p4[4][ix] = p4[ip][ix]; } }

  // Assign antitop momenta
  for (unsigned int ipar=0; ipar<antitopDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(antitopDaughters.at(ipar).second);
    if (antitopDaughters.size()==1){
      p4[3][0] = momTmp->T()*GeV;
      p4[3][1] = momTmp->X()*GeV;
      p4[3][2] = momTmp->Y()*GeV;
      p4[3][3] = momTmp->Z()*GeV;
    }
    // size==3
    else if (ipar==0){ // bb
      p4[5][0] = momTmp->T()*GeV;
      p4[5][1] = momTmp->X()*GeV;
      p4[5][2] = momTmp->Y()*GeV;
      p4[5][3] = momTmp->Z()*GeV;
    }
    else if (ipar==1){ // Wf
      p4[7][0] = momTmp->T()*GeV;
      p4[7][1] = momTmp->X()*GeV;
      p4[7][2] = momTmp->Y()*GeV;
      p4[7][3] = momTmp->Z()*GeV;
      p4[6][0] += p4[7][0];
      p4[6][1] += p4[7][1];
      p4[6][2] += p4[7][2];
      p4[6][3] += p4[7][3];
    }
    else/* if (ipar==1)*/{ // Wfb
      p4[8][0] = momTmp->T()*GeV;
      p4[8][1] = momTmp->X()*GeV;
      p4[8][2] = momTmp->Y()*GeV;
      p4[8][3] = momTmp->Z()*GeV;
      p4[6][0] += p4[8][0];
      p4[6][1] += p4[8][1];
      p4[6][2] += p4[8][2];
      p4[6][3] += p4[8][3];
    }
    MomStore[7] = MomStore[7] + (*momTmp); // MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }
  if (antitopDaughters.size()!=1){ for (int ix=0; ix<4; ix++){ for (int ip=5; ip<=6; ip++) p4[3][ix] = p4[ip][ix]; } }

  for (unsigned int ipar=0; ipar<mela_event.pDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(mela_event.pDaughters.at(ipar).second);
    p4[2][0] += momTmp->T()*GeV;
    p4[2][1] += momTmp->X()*GeV;
    p4[2][2] += momTmp->Y()*GeV;
    p4[2][3] += momTmp->Z()*GeV;
    MomStore[5] = MomStore[5] + (*momTmp); // i==(2, 3, 4) is (J1, J2, H), recorded as MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }

  if (verbosity >= TVar::DEBUG){
    for (int ii=0; ii<13; ii++){ cout << "p4[" << ii << "] = "; for (int jj=0; jj<4; jj++) cout << p4[ii][jj]/GeV << '\t'; cout << endl; }
  }

  double defaultRenScale = scale_.scale;
  double defaultFacScale = facscale_.facscale;
  //cout << "Default scales: " << defaultRenScale << '\t' << defaultFacScale << endl;
  int defaultNloop = nlooprun_.nlooprun;
  int defaultNflav = nflav_.nflav;
  string defaultPdflabel = pdlabel_.pdlabel;
  double renQ = InterpretScaleScheme(production, matrixElement, event_scales->renomalizationScheme, MomStore);
  //cout << "renQ: " << renQ << " x " << event_scales->ren_scale_factor << endl;
  double facQ = InterpretScaleScheme(production, matrixElement, event_scales->factorizationScheme, MomStore);
  //cout << "facQ: " << facQ << " x " << event_scales->fac_scale_factor << endl;
  SetAlphaS(renQ, facQ, event_scales->ren_scale_factor, event_scales->fac_scale_factor, 1, 5, "cteq6_l"); // Set AlphaS(|Q|/2, mynloop, mynflav, mypartonPDF)

  __modjhugenmela_MOD_settopdecays(&topDecay);
  __modttbhiggs_MOD_evalxsec_pp_ttbh(p4, &topProcess, MatElsq);
  if (isUnknown[0] && isUnknown[1]){
    for (unsigned int ix=0; ix<4; ix++){
      swap(p4[3][ix], p4[4][ix]);
      swap(p4[5][ix], p4[9][ix]);
      swap(p4[6][ix], p4[10][ix]);
      swap(p4[7][ix], p4[12][ix]);
      swap(p4[8][ix], p4[11][ix]);
    }
    __modttbhiggs_MOD_evalxsec_pp_ttbh(p4, &topProcess, MatElsq_tmp);
    for (int ix=0; ix<11; ix++){ for (int iy=0; iy<11; iy++) MatElsq[iy][ix] = (MatElsq[iy][ix]+MatElsq_tmp[iy][ix])/2.; }
  }
  int defaultTopDecay=-1;
  __modjhugenmela_MOD_settopdecays(&defaultTopDecay); // reset top decay

  sum_msqjk = SumMEPDF(MomStore[0], MomStore[1], MatElsq, RcdME, EBEAM, verbosity);

  //cout << "Before reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  SetAlphaS(defaultRenScale, defaultFacScale, 1., 1., defaultNloop, defaultNflav, defaultPdflabel); // Protection for other probabilities
  //cout << "Default scale reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  return sum_msqjk;
}

// bbH
double TUtil::BBHiggsMatEl(
  TVar::Process process, TVar::Production production, TVar::MatrixElement matrixElement,
  event_scales_type* event_scales, MelaIO* RcdME,
  double EBEAM,
  int botProcess,
  TVar::VerbosityLevel verbosity
  ){
  const double GeV=1./100.; // JHUGen mom. scale factor
  double sum_msqjk = 0;
  double MatElsq[nmsq][nmsq]={ { 0 } };
  double MatElsq_tmp[nmsq][nmsq]={ { 0 } };

  if (matrixElement!=TVar::JHUGen){ cerr << "TUtil::BBHiggsMatEl: Non-JHUGen MEs are not supported." << endl; return sum_msqjk; }
  if (production!=TVar::bbH){ cerr << "TUtil::BBHiggsMatEl: Only bbH is supported." << endl; return sum_msqjk; }

  int partIncCode=TVar::kUseAssociated_Jets; // Look for jets
  int nRequested_AssociatedJets=2;

  simple_event_record mela_event;
  mela_event.AssociationCode=partIncCode;
  mela_event.nRequested_AssociatedJets=nRequested_AssociatedJets;
  GetBoostedParticleVectors(
    RcdME->melaCand,
    mela_event
    );

  if (mela_event.pAssociated.size()<2){
    cerr
      << "TUtil::BBHiggsMatEl: Number of stable bs (" << mela_event.pAssociated.size() << ")"
      <<" in bbH process is not 2!" << endl;
    return sum_msqjk;
  }

  SimpleParticleCollection_t topDaughters;
  SimpleParticleCollection_t antitopDaughters;
  bool isUnknown[2]; isUnknown[0]=false; isUnknown[1]=false;

  if (mela_event.pAssociated.at(0).first>=0){ topDaughters.push_back(mela_event.pAssociated.at(0)); isUnknown[0]=(PDGHelpers::isAnUnknownJet(mela_event.pAssociated.at(0).first)); }
  else antitopDaughters.push_back(mela_event.pAssociated.at(0));
  if (mela_event.pAssociated.at(1).first<=0){ antitopDaughters.push_back(mela_event.pAssociated.at(1)); isUnknown[1]=(PDGHelpers::isAnUnknownJet(mela_event.pAssociated.at(1).first)); }
  else topDaughters.push_back(mela_event.pAssociated.at(1));

  // Start assigning the momenta
  // 0,1: p1 p2
  // 2-4: H,tb,t
  // 5-8: bb,W-,f,fb
  // 9-12: b,W+,fb,f
  double p4[13][4]={ { 0 } };
  double MYIDUP_prod[2]={ 0 };
  TLorentzVector MomStore[mxpart];
  for (int i = 0; i < mxpart; i++) MomStore[i].SetXYZT(0, 0, 0, 0);
  for (int ipar=0; ipar<2; ipar++){
    TLorentzVector* momTmp = &(mela_event.pMothers.at(ipar).second);
    int* idtmp = &(mela_event.pMothers.at(ipar).first);
    if (!PDGHelpers::isAnUnknownJet(*idtmp)) MYIDUP_prod[ipar] = *idtmp;
    else MYIDUP_prod[ipar] = 0;
    if (momTmp->T()>0.){
      p4[ipar][0] = -momTmp->T()*GeV;
      p4[ipar][1] = -momTmp->X()*GeV;
      p4[ipar][2] = -momTmp->Y()*GeV;
      p4[ipar][3] = -momTmp->Z()*GeV;
      MomStore[ipar] = (*momTmp);
    }
    else{
      p4[ipar][0] = momTmp->T()*GeV;
      p4[ipar][1] = momTmp->X()*GeV;
      p4[ipar][2] = momTmp->Y()*GeV;
      p4[ipar][3] = momTmp->Z()*GeV;
      MomStore[ipar] = -(*momTmp);
      MYIDUP_prod[ipar] = -MYIDUP_prod[ipar];
    }
  }

  // Assign b momenta
  for (unsigned int ipar=0; ipar<topDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(topDaughters.at(ipar).second);
    p4[4][0] = momTmp->T()*GeV;
    p4[4][1] = momTmp->X()*GeV;
    p4[4][2] = momTmp->Y()*GeV;
    p4[4][3] = momTmp->Z()*GeV;
    MomStore[6] = MomStore[6] + (*momTmp); // MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }

  // Assign bb momenta
  for (unsigned int ipar=0; ipar<antitopDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(antitopDaughters.at(ipar).second);
    p4[3][0] = momTmp->T()*GeV;
    p4[3][1] = momTmp->X()*GeV;
    p4[3][2] = momTmp->Y()*GeV;
    p4[3][3] = momTmp->Z()*GeV;
    MomStore[7] = MomStore[7] + (*momTmp); // MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }

  for (unsigned int ipar=0; ipar<mela_event.pDaughters.size(); ipar++){
    TLorentzVector* momTmp = &(mela_event.pDaughters.at(ipar).second);
    p4[2][0] += momTmp->T()*GeV;
    p4[2][1] += momTmp->X()*GeV;
    p4[2][2] += momTmp->Y()*GeV;
    p4[2][3] += momTmp->Z()*GeV;
    MomStore[5] = MomStore[5] + (*momTmp); // i==(2, 3, 4) is (J1, J2, H), recorded as MomStore (I1, I2, 0, 0, 0, H, J1, J2)
  }

  if (verbosity >= TVar::DEBUG){
    for (int ii=0; ii<13; ii++){ cout << "p4[" << ii << "] = "; for (int jj=0; jj<4; jj++) cout << p4[ii][jj]/GeV << '\t'; cout << endl; }
  }

  double defaultRenScale = scale_.scale;
  double defaultFacScale = facscale_.facscale;
  //cout << "Default scales: " << defaultRenScale << '\t' << defaultFacScale << endl;
  int defaultNloop = nlooprun_.nlooprun;
  int defaultNflav = nflav_.nflav;
  string defaultPdflabel = pdlabel_.pdlabel;
  double renQ = InterpretScaleScheme(production, matrixElement, event_scales->renomalizationScheme, MomStore);
  //cout << "renQ: " << renQ << " x " << event_scales->ren_scale_factor << endl;
  double facQ = InterpretScaleScheme(production, matrixElement, event_scales->factorizationScheme, MomStore);
  //cout << "facQ: " << facQ << " x " << event_scales->fac_scale_factor << endl;
  SetAlphaS(renQ, facQ, event_scales->ren_scale_factor, event_scales->fac_scale_factor, 1, 5, "cteq6_l"); // Set AlphaS(|Q|/2, mynloop, mynflav, mypartonPDF)

  __modttbhiggs_MOD_evalxsec_pp_bbbh(p4, &botProcess, MatElsq);
  if (isUnknown[0] && isUnknown[1]){
    for (unsigned int ix=0; ix<4; ix++) swap(p4[3][ix], p4[4][ix]);
    __modttbhiggs_MOD_evalxsec_pp_bbbh(p4, &botProcess, MatElsq_tmp);
    for (int ix=0; ix<11; ix++){ for (int iy=0; iy<11; iy++) MatElsq[iy][ix] = (MatElsq[iy][ix]+MatElsq_tmp[iy][ix])/2.; }
  }
  sum_msqjk = SumMEPDF(MomStore[0], MomStore[1], MatElsq, RcdME, EBEAM, verbosity);

  //cout << "Before reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  SetAlphaS(defaultRenScale, defaultFacScale, 1., 1., defaultNloop, defaultNflav, defaultPdflabel); // Protection for other probabilities
  //cout << "Default scale reset: " << scale_.scale << '\t' << facscale_.facscale << endl;
  return sum_msqjk;
}

// CheckPartonMomFraction computes xx[0:1] based on p0, p1
bool TUtil::CheckPartonMomFraction(const TLorentzVector p0, const TLorentzVector p1, double xx[2], double EBEAM, TVar::VerbosityLevel verbosity){
  //Make sure parton Level Energy fraction is [0,1]
  //phase space function already makes sure the parton energy fraction between [min,1]
  //  x0 EBeam =>   <= -x1 EBeam
  double sysPz=p0.Pz()    + p1.Pz();
  double sysE =p0.Energy()+ p1.Energy();

  //Ignore the Pt doesn't make significant effect
  //double sysPt_sqr=sysPx*sysPx+sysPy*sysPy;
  //if(sysPt_sqr>=1.0E-10)  sysE=TMath::Sqrt(sysE*sysE-sysPt_sqr);
  xx[0]=(sysE+sysPz)/EBEAM/2.;
  xx[1]=(sysE-sysPz)/EBEAM/2.;
  if (verbosity >= TVar::DEBUG) cout << "xx[0]: " << xx[0] << ", xx[1] = " << xx[1] << '\n';

  if (
    xx[0] > 1.0 || xx[0]<=xmin_.xmin
    ||
    xx[1] > 1.0 || xx[1]<=xmin_.xmin
    ) return false;
  else return true;
}
// ComputePDF does the PDF computation
void TUtil::ComputePDF(const TLorentzVector p0, const TLorentzVector p1, double fx1[nmsq], double fx2[nmsq], double EBEAM, TVar::VerbosityLevel verbosity){
  double xx[2]={ 0 };
  bool passPartonErgFrac=CheckPartonMomFraction(p0, p1, xx, EBEAM, verbosity);
  if (passPartonErgFrac){
    ///// USE JHUGEN SUBROUTINE (Accomodates LHAPDF) /////
    double fx1x2_jhu[2][13]={ { 0 } };
    __modkinematics_MOD_setpdfs(&(xx[0]), &(xx[1]), fx1x2_jhu);
    for (int ip=-6; ip<=6; ip++){
      int fac=0;
      if (ip!=0 && (abs(ip)%2==0)) fac=-1;
      else if (ip!=0) fac=1;
      if (ip<0) fac=-fac;
      int jp=ip+fac;
      fx1[jp]=fx1x2_jhu[0][ip];
      fx2[jp]=fx1x2_jhu[1][ip];
    }
    /*
    ///// USE MCFM SUBROUTINE fdist_linux /////
    //Calculate Pdf
    //Parton Density Function is always evalualted at pT=0 frame
    //Always pass address through fortran function
    fdist_(&density_.ih1, &xx[0], &facscale_.facscale, fx1);
    fdist_(&density_.ih2, &xx[1], &facscale_.facscale, fx2);
    */
  }
}
// SumMEPDF sums over all production parton flavors according to PDF and calls ComputePDF
double TUtil::SumMEPDF(const TLorentzVector p0, const TLorentzVector p1, double msq[nmsq][nmsq], MelaIO* RcdME, double EBEAM, TVar::VerbosityLevel verbosity){
  double fx1[nmsq]={ 0 };
  double fx2[nmsq]={ 0 };
  //double wgt_msq[nmsq][nmsq]={ { 0 } };

  ComputePDF(p0, p1, fx1, fx2, EBEAM, verbosity);
  RcdME->setPartonWeights(fx1, fx2);
  RcdME->setMEArray(msq,true);
  RcdME->computeWeightedMEArray();
  //RcdME->getWeightedMEArray(wgt_msq);
  return RcdME->getSumME();
}

// GetBoostedParticleVectors decomposes the MELACandidate object melaCand into mothers, daughters, associateds and tops
// and boosts them to the pT=0 frame for the particular mela_event.AssociationCode, which is a product of TVar::kUseAssociated* (prime numbers).
// If no mothers are present, it assigns mothers as Z>0, Z<0. If they are present, it orders them as "incoming" q-qbar / g-qbar / q-g / g-g
// Associated particles passed are different based on the code. If code==1, no associated particles are passed.
// mela_event.intermediateVids are needed to keep track of the decay mode. TVar::Process or TVar::Production do not keep track of the V/f decay modes.
// This is the major replacement functionality of the TVar lepton flavors.
void TUtil::GetBoostedParticleVectors(
  MELACandidate* melaCand,
  simple_event_record& mela_event
  ){
  // This is the beginning of one long function.

  int code = mela_event.AssociationCode;
  int aVhypo = mela_event.AssociationVCompatibility;
  TLorentzVector nullFourVector(0, 0, 0, 0);

  SimpleParticleCollection_t daughters;
  vector<int> idVstar;
  if (melaCand->getNDaughters()==0){
    // Undecayed Higgs has V1=H, V2=empty, no sortedDaughters!
    daughters.push_back(SimpleParticle_t(melaCand->id, melaCand->p4));
    idVstar.push_back(melaCand->id);
    idVstar.push_back(-9000);
  }
  else{
    // H->ffb has V1=f->f, V2=fb->fb
    // H->GG has V1=G->G, V2=G->G
    // H->ZG has V1=Z->ffb, V2=G->G
    // Everything else is as expected.
    for (int iv=0; iv<2; iv++){ // 2 Vs are guaranteed in MELACandidate.
      MELAParticle* Vdau = melaCand->getSortedV(iv);
      if (Vdau!=0){
        int idtmp = Vdau->id;
        for (int ivd=0; ivd<Vdau->getNDaughters(); ivd++){
          MELAParticle* Vdau_i = Vdau->getDaughter(ivd);
          if (Vdau_i!=0 && Vdau_i->passSelection) daughters.push_back(SimpleParticle_t(Vdau_i->id, Vdau_i->p4));
        }
        if (idtmp!=0 || Vdau->getNDaughters()>0){ // Avoid "empty" intermediate Vs of the MELACandidate object
          if (Vdau->getNDaughters()>=2 && PDGHelpers::isAPhoton(idtmp)) idtmp=23; // Special case to avoid V->2f with HVVmass==Zeromass setting (could happen by mistake)
          idVstar.push_back(idtmp);
        }
      }
      else idVstar.push_back(-9000);
    }
  }
  if (daughters.size()>=2){
    unsigned int nffs = daughters.size()/2;
    for (unsigned int iv=0; iv<nffs; iv++){
      TUtil::removeMassFromPair(
        daughters.at(2*iv+0).second, daughters.at(2*iv+0).first,
        daughters.at(2*iv+1).second, daughters.at(2*iv+1).first
        );
    }
    if (2*nffs<daughters.size()){
      TLorentzVector tmp = nullFourVector;
      TUtil::removeMassFromPair(
        daughters.at(daughters.size()-1).second, daughters.at(daughters.size()-1).first,
        tmp, -9000
        );
    }
  }

  /***** ASSOCIATED PARTICLES *****/
  int nsatisfied_jets=0;
  int nsatisfied_lnus=0;
  int nsatisfied_gammas=0;
  vector<MELAParticle*> candidateVs; // Used if aVhypo!=0
  SimpleParticleCollection_t associated;
  if (aVhypo!=0){

    for (int iv=2; iv<melaCand->getNSortedVs(); iv++){ // Loop over associated Vs
      MELAParticle* Vdau = melaCand->getSortedV(iv);
      if (Vdau!=0){
        bool doAdd=false;
        int idV = Vdau->id;
        if ((abs(idV)==aVhypo || idV==0) && Vdau->getNDaughters()>0){ // If the V is unknown or compatible with the requested hypothesis
          doAdd=true;
          for (int ivd=0; ivd<Vdau->getNDaughters(); ivd++){ // Loop over the daughters of V
            MELAParticle* Vdau_i = Vdau->getDaughter(ivd);
            if (Vdau_i==0){ doAdd=false; break; }
            else if (
              (mela_event.nRequested_AssociatedLeptons==0 && (PDGHelpers::isALepton(Vdau_i->id) || PDGHelpers::isANeutrino(Vdau_i->id)))
              ||
              (mela_event.nRequested_AssociatedJets==0 && PDGHelpers::isAJet(Vdau_i->id))
              ){
              doAdd=false; break;
            }
          }
        }
        if (doAdd) candidateVs.push_back(Vdau);
      }
    } // End loop over associated Vs

    cout << "TUtil::GetBoostedParticleVectors: candidateVs size = " << candidateVs.size() << endl;

    // Pick however many candidates necessary to fill up the requested number of jets or lepton(+)neutrinos
    for (unsigned int iv=0; iv<candidateVs.size(); iv++){
      MELAParticle* Vdau = candidateVs.at(iv);
      SimpleParticleCollection_t associated_tmp;
      for (int ivd=0; ivd<Vdau->getNDaughters(); ivd++){ // Loop over the daughters of V
        MELAParticle* part = Vdau->getDaughter(ivd);
        if (
          part->passSelection
          &&
          (PDGHelpers::isALepton(part->id) || PDGHelpers::isANeutrino(part->id))
          &&
          nsatisfied_lnus<mela_event.nRequested_AssociatedLeptons
          ){
          nsatisfied_lnus++;
          associated_tmp.push_back(SimpleParticle_t(part->id, part->p4));
        }
        else if (
          part->passSelection
          &&
          PDGHelpers::isAJet(part->id)
          &&
          nsatisfied_jets<mela_event.nRequested_AssociatedJets
          ){
          nsatisfied_jets++;
          associated_tmp.push_back(SimpleParticle_t(part->id, part->p4));
        }
      }
      // Add the id of the intermediate V into the idVstar array
      if (associated_tmp.size()>=2 || (associated_tmp.size()==1 && PDGHelpers::isAPhoton(associated_tmp.at(0).first))) idVstar.push_back(Vdau->id); // Only add the V-id of pairs that passed
      // Adjust the kinematics of associated V-originating particles
      if (associated_tmp.size()>=2){ // ==1 means a photon, so omit it here.
        unsigned int nffs = associated_tmp.size()/2;
        for (unsigned int iv=0; iv<nffs; iv++){
          TUtil::removeMassFromPair(
            associated_tmp.at(2*iv+0).second, associated_tmp.at(2*iv+0).first,
            associated_tmp.at(2*iv+1).second, associated_tmp.at(2*iv+1).first
            );
        }
        if (2*nffs<associated_tmp.size()){
          TLorentzVector tmp = nullFourVector;
          TUtil::removeMassFromPair(
            associated_tmp.at(associated_tmp.size()-1).second, associated_tmp.at(associated_tmp.size()-1).first,
            tmp, -9000
            );
        }
      }
      for (unsigned int ias=0; ias<associated_tmp.size(); ias++) associated.push_back(associated_tmp.at(ias)); // Fill associated at the last step
    }

  }
  else{ // Could be split to aVhypo==0 and aVhypo<0 if associated V+jets is needed
    // Associated leptons
    if (code%TVar::kUseAssociated_Leptons==0){
      SimpleParticleCollection_t associated_tmp;
      for (int ip=0; ip<melaCand->getNAssociatedLeptons(); ip++){
        MELAParticle* part = melaCand->getAssociatedLepton(ip);
        if (part!=0 && part->passSelection && nsatisfied_lnus<mela_event.nRequested_AssociatedLeptons){
          nsatisfied_lnus++;
          associated_tmp.push_back(SimpleParticle_t(part->id, part->p4));
        }
      }
      // Adjust the kinematics of associated non-V-originating particles
      if (associated_tmp.size()>=1){
        unsigned int nffs = associated_tmp.size()/2;
        for (unsigned int iv=0; iv<nffs; iv++){
          TUtil::removeMassFromPair(
            associated_tmp.at(2*iv+0).second, associated_tmp.at(2*iv+0).first,
            associated_tmp.at(2*iv+1).second, associated_tmp.at(2*iv+1).first
            );
        }
        if (2*nffs<associated_tmp.size()){
          TLorentzVector tmp = nullFourVector;
          TUtil::removeMassFromPair(
            associated_tmp.at(associated_tmp.size()-1).second, associated_tmp.at(associated_tmp.size()-1).first,
            tmp, -9000
            );
        }
      }
      for (unsigned int ias=0; ias<associated_tmp.size(); ias++) associated.push_back(associated_tmp.at(ias)); // Fill associated at the last step
    }
    // Associated jets
    if (code%TVar::kUseAssociated_Jets==0){
      SimpleParticleCollection_t associated_tmp;
      for (int ip=0; ip<melaCand->getNAssociatedJets(); ip++){
        MELAParticle* part = melaCand->getAssociatedJet(ip);
        if (part!=0 && part->passSelection && nsatisfied_jets<mela_event.nRequested_AssociatedJets){
          nsatisfied_jets++;
          associated_tmp.push_back(SimpleParticle_t(part->id, part->p4));
        }
      }
      // Adjust the kinematics of associated non-V-originating particles
      if (associated_tmp.size()>=1){
        unsigned int nffs = associated_tmp.size()/2;
        for (unsigned int iv=0; iv<nffs; iv++){
          TUtil::removeMassFromPair(
            associated_tmp.at(2*iv+0).second, associated_tmp.at(2*iv+0).first,
            associated_tmp.at(2*iv+1).second, associated_tmp.at(2*iv+1).first
            );
        }
        if (2*nffs<associated_tmp.size()){
          TLorentzVector tmp = nullFourVector;
          TUtil::removeMassFromPair(
            associated_tmp.at(associated_tmp.size()-1).second, associated_tmp.at(associated_tmp.size()-1).first,
            tmp, -9000
            );
        }
      }
      for (unsigned int ias=0; ias<associated_tmp.size(); ias++) associated.push_back(associated_tmp.at(ias)); // Fill associated at the last step
    }

  } // End if(aVhypo!=0)-else statement

  if (code%TVar::kUseAssociated_Photons==0){
    for (int ip=0; ip<melaCand->getNAssociatedPhotons(); ip++){
      MELAParticle* part = melaCand->getAssociatedPhoton(ip);
      if (part!=0 && part->passSelection && nsatisfied_gammas<mela_event.nRequested_AssociatedPhotons){
        nsatisfied_gammas++;
        associated.push_back(SimpleParticle_t(part->id, part->p4));
      }
    }
  }
  /***** END ASSOCIATED PARTICLES *****/


  /***** ASSOCIATED TOP OBJECTS *****/
  int nsatisfied_tops=0;
  int nsatisfied_antitops=0;
  vector<SimpleParticleCollection_t> topDaughters;
  vector<SimpleParticleCollection_t> antitopDaughters;
  SimpleParticleCollection_t stableTops;
  SimpleParticleCollection_t stableAntitops;

  vector<MELATopCandidate*> tops;
  vector<MELATopCandidate*> topbars;
  vector<MELATopCandidate*> unknowntops;
  if (code%TVar::kUseAssociated_StableTops==0 && code%TVar::kUseAssociated_UnstableTops==0){ cerr << "TUtil::GetBoostedParticleVectors: Stable and unstable tops ar not supported at the same time!"  << endl; }
  else if (code%TVar::kUseAssociated_StableTops==0 || code%TVar::kUseAssociated_UnstableTops==0){

    for (int itop=0; itop<melaCand->getNAssociatedTops(); itop++){
      MELATopCandidate* theTop = melaCand->getAssociatedTop(itop);
      if (theTop!=0 && theTop->passSelection){
        vector<MELATopCandidate*>* particleArray;
        if (theTop->id==6) particleArray = &tops;
        else if (theTop->id==-6) particleArray = &topbars;
        else particleArray = &unknowntops;
        if (
          (code%TVar::kUseAssociated_StableTops==0)
          ||
          (theTop->getNDaughters()==3 && code%TVar::kUseAssociated_UnstableTops==0)
          ) particleArray->push_back((MELATopCandidate*)theTop);
      }
    }

    // Fill the stable/unstable top arrays
    for (unsigned int itop=0; itop<tops.size(); itop++){
      MELATopCandidate* theTop = tops.at(itop); // Checks are already performed
      if (code%TVar::kUseAssociated_StableTops==0 && nsatisfied_tops<mela_event.nRequested_Tops){ // Case with no daughters needed
        nsatisfied_tops++;
        stableTops.push_back(SimpleParticle_t(theTop->id, theTop->p4));
      }
      else if (code%TVar::kUseAssociated_UnstableTops==0 && theTop->getNDaughters()==3 && nsatisfied_tops<mela_event.nRequested_Tops){ // Case with daughters needed
        nsatisfied_tops++;
        SimpleParticleCollection_t vdaughters;

        MELAParticle* bottom = theTop->getLightQuark();
        MELAParticle* Wf = theTop->getWFermion();
        MELAParticle* Wfb = theTop->getWAntifermion();
        if (bottom!=0) vdaughters.push_back(SimpleParticle_t(bottom->id, bottom->p4));
        if (Wf!=0) vdaughters.push_back(SimpleParticle_t(Wf->id, Wf->p4));
        if (Wfb!=0) vdaughters.push_back(SimpleParticle_t(Wfb->id, Wfb->p4));

        TUtil::adjustTopDaughters(vdaughters); // Adjust top daughter kinematics
        if (vdaughters.size()==3) topDaughters.push_back(vdaughters);
      }
    }
    for (unsigned int itop=0; itop<topbars.size(); itop++){
      MELATopCandidate* theTop = topbars.at(itop);
      if (code%TVar::kUseAssociated_StableTops==0 && nsatisfied_antitops<mela_event.nRequested_Antitops){ // Case with no daughters needed
        nsatisfied_antitops++;
        stableAntitops.push_back(SimpleParticle_t(theTop->id, theTop->p4));
      }
      else if (code%TVar::kUseAssociated_UnstableTops==0 && nsatisfied_antitops<mela_event.nRequested_Antitops){ // Case with daughters needed
        nsatisfied_antitops++;
        SimpleParticleCollection_t vdaughters;

        MELAParticle* bottom = theTop->getLightQuark();
        MELAParticle* Wf = theTop->getWFermion();
        MELAParticle* Wfb = theTop->getWAntifermion();
        if (bottom!=0) vdaughters.push_back(SimpleParticle_t(bottom->id, bottom->p4));
        if (Wf!=0) vdaughters.push_back(SimpleParticle_t(Wf->id, Wf->p4));
        if (Wfb!=0) vdaughters.push_back(SimpleParticle_t(Wfb->id, Wfb->p4));

        TUtil::adjustTopDaughters(vdaughters); // Adjust top daughter kinematics
        if (vdaughters.size()==3) antitopDaughters.push_back(vdaughters);
      }
      else break;
    }
    if (unknowntops.size()!=0){ // Loops over te unknown-id tops
      // Fill tops, then antitops from the unknown tops
      for (unsigned int itop=0; itop<unknowntops.size(); itop++){
        MELATopCandidate* theTop = unknowntops.at(itop);
        // t, then tb cases with no daughters needed
        if (code%TVar::kUseAssociated_StableTops==0 && nsatisfied_tops<mela_event.nRequested_Tops){
          nsatisfied_tops++;
          stableTops.push_back(SimpleParticle_t(theTop->id, theTop->p4));
        }
        else if (code%TVar::kUseAssociated_StableTops==0 && nsatisfied_antitops<mela_event.nRequested_Antitops){
          nsatisfied_antitops++;
          stableAntitops.push_back(SimpleParticle_t(theTop->id, theTop->p4));
        }
        // t, then tb cases with daughters needed
        else if (code%TVar::kUseAssociated_UnstableTops==0 && nsatisfied_tops<mela_event.nRequested_Tops){
          nsatisfied_tops++;
          SimpleParticleCollection_t vdaughters;

          MELAParticle* bottom = theTop->getLightQuark();
          MELAParticle* Wf = theTop->getWFermion();
          MELAParticle* Wfb = theTop->getWAntifermion();
          if (bottom!=0) vdaughters.push_back(SimpleParticle_t(bottom->id, bottom->p4));
          if (Wf!=0) vdaughters.push_back(SimpleParticle_t(Wf->id, Wf->p4));
          if (Wfb!=0) vdaughters.push_back(SimpleParticle_t(Wfb->id, Wfb->p4));

          TUtil::adjustTopDaughters(vdaughters); // Adjust top daughter kinematics
          if (vdaughters.size()==3) topDaughters.push_back(vdaughters);
        }
        else if (code%TVar::kUseAssociated_UnstableTops==0 && nsatisfied_antitops<mela_event.nRequested_Antitops){
          nsatisfied_antitops++;
          SimpleParticleCollection_t vdaughters;

          MELAParticle* bottom = theTop->getLightQuark();
          MELAParticle* Wf = theTop->getWFermion();
          MELAParticle* Wfb = theTop->getWAntifermion();
          if (bottom!=0) vdaughters.push_back(SimpleParticle_t(bottom->id, bottom->p4));
          if (Wf!=0) vdaughters.push_back(SimpleParticle_t(Wf->id, Wf->p4));
          if (Wfb!=0) vdaughters.push_back(SimpleParticle_t(Wfb->id, Wfb->p4));

          TUtil::adjustTopDaughters(vdaughters); // Adjust top daughter kinematics
          if (vdaughters.size()==3) antitopDaughters.push_back(vdaughters);
        }
      }
    }

  }
  /***** END ASSOCIATED TOP OBJECTS *****/


  /***** BOOSTS TO THE CORRECT PT=0 FRAME *****/
  // Gather all final state particles collected for this frame
  TLorentzVector pTotal(0, 0, 0, 0);
  for (unsigned int ip=0; ip<daughters.size(); ip++) pTotal = pTotal + daughters.at(ip).second;
  for (unsigned int ip=0; ip<associated.size(); ip++) pTotal = pTotal + associated.at(ip).second;
  for (unsigned int ip=0; ip<stableTops.size(); ip++) pTotal = pTotal + stableTops.at(ip).second;
  for (unsigned int ip=0; ip<stableAntitops.size(); ip++) pTotal = pTotal + stableAntitops.at(ip).second;
  for (unsigned int ip=0; ip<topDaughters.size(); ip++){ for (unsigned int ipd=0; ipd<topDaughters.at(ip).size(); ipd++) pTotal = pTotal + topDaughters.at(ip).at(ipd).second; }
  for (unsigned int ip=0; ip<antitopDaughters.size(); ip++){ for (unsigned int ipd=0; ipd<antitopDaughters.at(ip).size(); ipd++) pTotal = pTotal + antitopDaughters.at(ip).at(ipd).second; }

  // Get the boost vector and boost all final state particles
  double qX = pTotal.X();
  double qY = pTotal.Y();
  double qE = pTotal.T();;
  if ((qX*qX+qY*qY)>0.){
    TVector3 boostV(-qX/qE, -qY/qE, 0.);
    for (unsigned int ip=0; ip<daughters.size(); ip++) daughters.at(ip).second.Boost(boostV);
    for (unsigned int ip=0; ip<associated.size(); ip++) associated.at(ip).second.Boost(boostV);
    for (unsigned int ip=0; ip<stableTops.size(); ip++) stableTops.at(ip).second.Boost(boostV);
    for (unsigned int ip=0; ip<stableAntitops.size(); ip++) stableAntitops.at(ip).second.Boost(boostV);
    for (unsigned int ip=0; ip<topDaughters.size(); ip++){ for (unsigned int ipd=0; ipd<topDaughters.at(ip).size(); ipd++) topDaughters.at(ip).at(ipd).second.Boost(boostV); }
    for (unsigned int ip=0; ip<antitopDaughters.size(); ip++){ for (unsigned int ipd=0; ipd<antitopDaughters.at(ip).size(); ipd++) antitopDaughters.at(ip).at(ipd).second.Boost(boostV); }
    pTotal.Boost(boostV);
  }

  // Mothers need special treatment:
  // In case they are undefined, mother id is unknown, and mothers are ordered as M1==(pz>0) and M2==(pz<0).
  // In case they are defined, mothers are first matched to the assumed momenta through their pz's.
  // They are ordered by M1==f, M2==fb afterward if this is possible.
  // Notice that the momenta of the mother objects are not actually used. If the event is truly a gen. particle, everything would be in the pT=0 frame to begin with, and everybody is happy in reweighting.
  double sysPz= pTotal.Z();
  double sysE = pTotal.T();
  double pz0 = (sysE+sysPz)/2.;
  double pz1 = -(sysE-sysPz)/2.;
  int motherId[2]={ 0, 0 };
  if (melaCand->getNMothers()==2){
    // Match the assumed M1==(pz>0) and M2==(pz<0) to the pz of the actual mothers.
    if ((melaCand->getMother(0)->p4).Z()<0.){ // Swap by Z to get the correct momentum matching default M1, M2
      double pztmp = pz1;
      pz1=pz0;
      pz0=pztmp;
    }
    if (motherId[0]<0){ // Swap the ids of mothers and their "assumed" momenta to achieve ordering as "incoming" q-qbar
      double pztmp = pz1;
      pz1=pz0;
      pz0=pztmp;
      for (int ip=0; ip<2; ip++) motherId[1-ip]=melaCand->getMother(ip)->id;
    }
    else{ // No swap of mothers necessary, just get their ids
      for (int ip=0; ip<2; ip++) motherId[ip]=melaCand->getMother(ip)->id;
    }
  }
  TLorentzVector pM[2];
  pM[0].SetXYZT(0., 0., pz0, fabs(pz0));
  pM[1].SetXYZT(0., 0., pz1, fabs(pz1));

  // Fill the ids of the V intermediates to the candidate daughters
  mela_event.intermediateVid.clear();
  for (unsigned int ip=0; ip<idVstar.size(); ip++) mela_event.intermediateVid.push_back(idVstar.at(ip));
  // Fill the mothers
  mela_event.pMothers.clear();
  for (unsigned int ip=0; ip<2; ip++){ mela_event.pMothers.push_back(SimpleParticle_t(motherId[ip], pM[ip])); }
  // Fill the daughters
  mela_event.pDaughters.clear();
  for (unsigned int ip=0; ip<daughters.size(); ip++) mela_event.pDaughters.push_back(daughters.at(ip));
  // Fill the associated particles
  mela_event.pAssociated.clear();
  for (unsigned int ip=0; ip<associated.size(); ip++) mela_event.pAssociated.push_back(associated.at(ip));
  // Fill the stable tops and antitops
  mela_event.pStableTops.clear();
  for (unsigned int ip=0; ip<stableTops.size(); ip++) mela_event.pStableTops.push_back(stableTops.at(ip));
  mela_event.pStableAntitops.clear();
  for (unsigned int ip=0; ip<stableAntitops.size(); ip++) mela_event.pStableAntitops.push_back(stableAntitops.at(ip));
  // Fill the daughters of unstable tops and antitops
  mela_event.pTopDaughters.clear();
  for (unsigned int ip=0; ip<topDaughters.size(); ip++) mela_event.pTopDaughters.push_back(topDaughters.at(ip));
  mela_event.pAntitopDaughters.clear();
  for (unsigned int ip=0; ip<antitopDaughters.size(); ip++) mela_event.pAntitopDaughters.push_back(antitopDaughters.at(ip));

  // This is the end of one long function.
}

// Convert vectors of simple particles to MELAParticles and create a MELACandidate
// The output lists could be members of TEvtProb directly.
MELACandidate* TUtil::ConvertVectorFormat(
  // Inputs
  SimpleParticleCollection_t* pDaughters,
  SimpleParticleCollection_t* pAssociated,
  SimpleParticleCollection_t* pMothers,
  bool isGen,
  // Outputs
  std::vector<MELAParticle*>* particleList,
  std::vector<MELACandidate*>* candList
  ){
  MELACandidate* cand=0;

  if (pDaughters==0){ cerr << "TUtil::ConvertVectorFormat: No daughters!" << endl; return cand; }
  else if (pDaughters->size()==0){ cerr << "TUtil::ConvertVectorFormat: Daughter size==0!" << endl; return cand; }
  else if (pDaughters->size()>4){ cerr << "TUtil::ConvertVectorFormat: Daughter size " << pDaughters->size() << ">4 is not supported!" << endl; return cand; }
  if (pMothers!=0 && pMothers->size()!=2){ cerr << "TUtil::ConvertVectorFormat: Mothers momentum size (" << pMothers->size() << ") has to have had been 2! Continuing by omitting mothers." << endl; /*return cand;*/ }

  // Create mother, daughter and associated particle MELAParticle objects
  std::vector<MELAParticle*> daughters;
  std::vector<MELAParticle*> aparticles;
  std::vector<MELAParticle*> mothers;
  for (unsigned int ip=0; ip<pDaughters->size(); ip++){
    MELAParticle* onePart = new MELAParticle((pDaughters->at(ip)).first, (pDaughters->at(ip)).second);
    onePart->setGenStatus(1); // Final state status
    if (particleList!=0) particleList->push_back(onePart);
    daughters.push_back(onePart);
  }
  if (pAssociated!=0){
    for (unsigned int ip=0; ip<pAssociated->size(); ip++){
      MELAParticle* onePart = new MELAParticle((pAssociated->at(ip)).first, (pAssociated->at(ip)).second);
      onePart->setGenStatus(1); // Final state status
      if (particleList!=0) particleList->push_back(onePart);
      aparticles.push_back(onePart);
    }
  }
  if (pMothers!=0 && pMothers->size()==2){
    for (unsigned int ip=0; ip<pMothers->size(); ip++){
      MELAParticle* onePart = new MELAParticle((pMothers->at(ip)).first, (pMothers->at(ip)).second);
      onePart->setGenStatus(-1); // Mother status
      if (particleList!=0) particleList->push_back(onePart);
      mothers.push_back(onePart);
    }
  }

  // Create the candidate
  /***** Adaptation of LHEAnalyzer::Event::constructVVCandidates *****/
  /*
  The assumption is that the daughters make sense for either ffb, gamgam, Zgam, ZZ or WW.
  No checking is done on whether particle-antiparticle pairing is correct when necessary.
  If not, you will get a seg. fault!
  */

  // Undecayed Higgs
  if (daughters.size()==1) cand = new MELACandidate(25, (daughters.at(0))->p4); // No sorting!
  // GG / ff final states
  else if (daughters.size()==2){
    MELAParticle* F1 = daughters.at(0);
    MELAParticle* F2 = daughters.at(1);
    TLorentzVector pH = F1->p4+F2->p4;
    cand = new MELACandidate(25, pH);
    cand->addDaughter(F1);
    cand->addDaughter(F2);
    double defaultHVVmass = PDGHelpers::HVVmass;
    PDGHelpers::setHVVmass(PDGHelpers::Zeromass);
    cand->sortDaughters();
    PDGHelpers::setHVVmass(defaultHVVmass);
  }
  // ZG / WG
  else if (daughters.size()==3){
    MELAParticle* F1 = daughters.at(0);
    MELAParticle* F2 = daughters.at(1);
    MELAParticle* gamma = daughters.at(2);
    if (PDGHelpers::isAPhoton(F1->id)){
      MELAParticle* tmp = F1;
      F1 = gamma;
      gamma = tmp;
    }
    else if (PDGHelpers::isAPhoton(F2->id)){
      MELAParticle* tmp = F2;
      F2 = gamma;
      gamma = tmp;
    }
    TLorentzVector pH = F1->p4+F2->p4+gamma->p4;
    double charge = F1->charge()+F2->charge()+gamma->charge();
    cand = new MELACandidate(25, pH);
    cand->addDaughter(F1);
    cand->addDaughter(F2);
    cand->addDaughter(gamma);
    double defaultHVVmass = PDGHelpers::HVVmass;
    if (fabs(charge)<0.01) PDGHelpers::setHVVmass(PDGHelpers::Zmass); // ZG
    else PDGHelpers::setHVVmass(PDGHelpers::Wmass); // WG,GW (?), un-tested
    cand->sortDaughters();
    PDGHelpers::setHVVmass(defaultHVVmass);
  }
  // ZZ / WW / ZW
  else/* if (daughters.size()==4)*/{
    TLorentzVector pH(0, 0, 0, 0);
    double charge = 0.;
    for (int ip=0; ip<4; ip++){ pH = pH + (daughters.at(ip))->p4; charge += (daughters.at(ip))->charge(); }
    cand = new MELACandidate(25, pH);
    for (int ip=0; ip<4; ip++) cand->addDaughter(daughters.at(ip));
    double defaultHVVmass = PDGHelpers::HVVmass;
    if (fabs(charge)>0.01) PDGHelpers::setHVVmass(PDGHelpers::Wmass); // WZ,ZW (?), un-tested
    cand->sortDaughters();
    PDGHelpers::setHVVmass(defaultHVVmass);
  }
  /***** Adaptation of LHEAnalyzer::Event::addVVCandidateMother *****/
  if (mothers.size()>0){ // ==2
    for (unsigned int ip=0; ip<mothers.size(); ip++) cand->addMother(mothers.at(ip));
    if (isGen) cand->setGenStatus(-1); // Candidate is a gen. particle!
  }
  /***** Adaptation of LHEAnalyzer::Event::addVVCandidateAppendages *****/
  if (aparticles.size()>0){
    for (unsigned int ip=0; ip<aparticles.size(); ip++){
      const int partId = (aparticles.at(ip))->id;
      if (PDGHelpers::isALepton(partId)) cand->addAssociatedLeptons(aparticles.at(ip));
      else if (PDGHelpers::isANeutrino(partId)) cand->addAssociatedNeutrinos(aparticles.at(ip)); // Be careful: Neutrinos are neutrinos, but also "leptons" in MELACandidate!
      else if (PDGHelpers::isAPhoton(partId)) cand->addAssociatedPhotons(aparticles.at(ip));
      else if (PDGHelpers::isAJet(partId)) cand->addAssociatedJets(aparticles.at(ip));
    }
    cand->addAssociatedVs(); // For the VH topology
  }

  if (candList!=0 && cand!=0) candList->push_back(cand);
  return cand;
}


// Convert the vector of top daughters (as simple particles) to MELAParticles and create a MELATopCandidate
// The output lists could be members of TEvtProb directly.
MELATopCandidate* TUtil::ConvertTopCandidate(
  // Input
  SimpleParticleCollection_t* TopDaughters,
  // Outputs
  std::vector<MELAParticle*>* particleList,
  std::vector<MELATopCandidate*>* topCandList
  ){
  MELATopCandidate* cand=0;

  if (TopDaughters==0){ cerr << "TUtil::ConvertTopCandidate: No daughters!" << endl; return cand; }
  else if (TopDaughters->size()==0){ cerr << "TUtil::ConvertTopCandidate: Daughter size==0!" << endl; return cand; }
  else if (!(TopDaughters->size()==1 || TopDaughters->size()==3)){ cerr << "TUtil::ConvertVectorFormat: Daughter size " << TopDaughters->size() << "!=1 or 3 is not supported!" << endl; return cand; }

  if (TopDaughters->size()==1){
    if (abs((TopDaughters->at(0)).first)==6 || (TopDaughters->at(0)).first==0){
      cand = new MELATopCandidate((TopDaughters->at(0)).first, (TopDaughters->at(0)).second);
      topCandList->push_back(cand);
    }
  }
  else if (TopDaughters->size()==3){
    MELAParticle* bottom = new MELAParticle((TopDaughters->at(0)).first, (TopDaughters->at(0)).second);
    MELAParticle* Wf = new MELAParticle((TopDaughters->at(0)).first, (TopDaughters->at(0)).second);
    MELAParticle* Wfb = new MELAParticle((TopDaughters->at(0)).first, (TopDaughters->at(0)).second);

    if (Wf->id<0 || Wfb->id>0){
      MELAParticle* parttmp = Wf;
      Wf=Wfb;
      Wfb=parttmp;
    }

    particleList->push_back(bottom);
    particleList->push_back(Wf);
    particleList->push_back(Wfb);

    cand = new MELATopCandidate(bottom, Wf, Wfb);
    topCandList->push_back(cand);
  }
  return cand;
}


