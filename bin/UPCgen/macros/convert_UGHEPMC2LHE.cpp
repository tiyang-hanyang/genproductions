#include <iostream>
#include <fstream>
#include <vector>
#include "Math/Vector4D.h"


void convert_UGHEPMC2LHE(const std::string& inFileName, const double& beamE1, const double& beamE2)
{
  // Open input file
  std::ifstream inFile(inFileName);
  if (not inFile.is_open())
    throw std::logic_error("[convert_UGHEPMC2LHE] Failed to open input file: "+inFileName);

  // Extract cross section
  double fidxsec(1), totxsec(3);
  std::ifstream xsecFile(inFileName.substr(0, inFileName.rfind("/")+1)+"xsec.out");
  if (xsecFile.is_open())
    xsecFile >> fidxsec >> totxsec;

  // Create output file
  const auto outFileName = inFileName.substr(0, inFileName.rfind(".")).substr(inFileName.rfind("/")+1) + ".lhe";
  std::ofstream outFile(outFileName);
  if (not outFile.is_open())
    throw std::logic_error("[convert_UGHEPMC2LHE] Failed to open output file: "+outFileName);

  std::cout << "Converting UPCGen HEPMC output to LHE format" << std::endl;

  // Add header
  outFile << "<LesHouchesEvents version=\"3.0\">" << std::endl;
  outFile << "<!-- " << std::endl << " #Converted from UPCGEN generator HEPMC output " << std::endl << "-->" << std::endl;
  outFile << "<header>" << std::endl << "</header>" << std::endl;

  // Put generic initialization-level info since UPCGen doesn't save this
  outFile << "<init>" << std::endl;
  outFile << std::fixed << std::setprecision(8) << std::scientific;
  //LHE format: https://arxiv.org/pdf/hep-ph/0109068.pdf
  //beam pdg id (1, 2), beam energy [GeV] (1, 2), PDF author group (1, 2), PDF set id (1, 2), weight strategy, # subprocesses
  outFile << "2212 2212 " << beamE1 << " " << beamE2 << " 0 0 0 0 3 1" << std::endl;
  //cross section [pb], cross section stat. unc. [pb], maximum event weight, subprocess id
  outFile << fidxsec << " " << 0.0 << " " << totxsec << " 81" << std::endl;
  outFile << "</init>" << std::endl;

  int nEvt(0);
  std::string line, label;

  // Read input file
  while (getline(inFile, line) &&
         line.rfind("END_EVENT_LISTING")==std::string::npos) {
    // Read event line
    if (line.rfind("E ", 0)!=0) continue;

    // E n nvertices nparticles
    int iEvt, nVtx, nPar;
	if (not (std::istringstream(line) >> label >> iEvt >> nVtx >> nPar) || label != "E")
      throw std::logic_error("[convert_UGHEPMC2LHE] Failed to parse event line: "+line);

    // U momentum_unit length_unit
    if (not getline(inFile, line) || not (std::istringstream(line) >> label) || label != "U")
      throw std::logic_error("[convert_UGHEPMC2LHE] Failed to parse event line: "+line);

    // Particle tuple: pdg id, status, mother index, 4-momentum
    std::vector<std::tuple<int, int, int, ROOT::Math::PxPyPzEVector>> parV(nPar);

    // Read particle lines
    for (size_t iPar=0; iPar < nPar; iPar++) {
      // P n nv pdgid px py pz e mass status
      double px, py, pz, en, mass;
      int parI, momI, pdgId, status;
      if (not getline(inFile, line) || not (std::istringstream(line) >> label >> parI >> momI >> pdgId >> px >> py >> pz >> en >> mass >> status) || label != "P" || parI != iPar+1 || momI >= parI)
        throw std::logic_error("[convert_UGHEPMC2LHE] Failed to parse track line: "+line);

      parV[iPar] = {pdgId, 1, momI, ROOT::Math::PxPyPzEVector(px, py, pz, en)};
      if (momI > 0)
        std::get<1>(parV[momI-1]) = 2;
    }

    // Add fake photons
    ROOT::Math::PxPyPzMVector p4(0,0,0,0);
    for (const auto& pV : parV)
      if (std::get<1>(pV) == 1)
        p4 += std::get<3>(pV);
    parV.emplace(parV.begin(), 22, -1, 0, ROOT::Math::PxPyPzMVector(0, 0, (p4.Z() + p4.E())/2., 0));
    parV.emplace(parV.begin(), 22, -1, 0, ROOT::Math::PxPyPzMVector(0, 0, (p4.Z() - p4.E())/2., 0));

    // Write event
    outFile << "<event>" << std::endl;
    //# particles, subprocess id, event weight, event scale, alpha_em, alpha_s
    outFile << parV.size() << " 81 1.0 -1.0 -1.0 -1.0" << std::endl;
    outFile << std::fixed << std::setprecision(10) << std::scientific;
    for (const auto& pV : parV) {
      const auto& [pdgId, status, momI, p] = pV;
      //particle: pdg id, status, mother index (1, 2), color flow tag (1, 2), (px, py, pz, energy, mass [GeV]), proper lifetime [mm], spin
      const auto momS = status > 0 ? (momI == 0 ? "1 2" : std::to_string(momI+2)+" 0") : "0 0";
      outFile << pdgId << " " << status << " " << momS << " 0 0 " << p.Px() << " " << p.Py() << " " << p.Pz() << " " << p.E() << " " << p.M() << " 0.0000e+00 9.0000e+00" << std::endl;
    }
    outFile << "</event>" << std::endl;
    nEvt++;
  }

  outFile << "</LesHouchesEvents>" << std::endl;
  outFile.close();
  inFile.close();
  std::cout << nEvt << " events written in " << outFileName << std::endl;
};


int main(int argc, char *argv[])
{
  if (argc != 4) {
    std::cout << "Invalid input parameters!" << std::endl;
    std::cout << "Usage: ./convert_UGHEPMC2LHE <INPUT_FILE> <BEAM_1_E> <BEAM_2_E>" << std::endl;
    return 1;
  }
  convert_UGHEPMC2LHE(std::string(argv[1]), std::atof(argv[2]), std::atof(argv[3]));
  return 0;
};
