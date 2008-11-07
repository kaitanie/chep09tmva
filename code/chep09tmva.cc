#include<iostream>
#include<fstream>
#include<string>
#include<vector>

#include <TFile.h>
#include <TString.h>
#include <TSystem.h>

#include <TMVA/Factory.h>

bool parseConf(std::string filename, std::vector<std::string>& variables,
               std::vector<std::string>& cuts, std::string& trainer,
               std::vector<std::pair<std::string, std::string> >& classifiers);
std::vector<std::string> preprocessLine(std::string& line);
TMVA::Types::EMVA getType(std::string desc);

// See more usage examples about TMVA training in tmva/TMVA/examples/TMVAnalysis.C
int main(int argc, char **argv) {
  // Configuration
  TString inputfileName("mva-input.root");
  TString outputfileName("TMVA.root");

  double signalWeight     = 1.0;
  double backgroundWeight = 1.0;

  // parse configuration
  std::string confFile("tmva.conf");
  if(argc > 1) {
    confFile = argv[1];
  }
  std::vector<std::string> variables;
  std::vector<std::string> cuts;
  std::vector<std::pair<std::string, std::string> > classifiers;
  std::string trainer;

  // Cuts
  TCut cut_s = "";

  if(!parseConf(confFile, variables, cuts, trainer, classifiers))
    return 1;

  std::cout << "Variables:" << std::endl;
  for(std::vector<std::string>::const_iterator iter = variables.begin();
      iter != variables.end(); ++iter) {
    std::cout << *iter << std::endl;
  }
  std::cout << std::endl;

  std::cout << "Cuts (applied to signal and bkg)" << std::endl;
  for(std::vector<std::string>::const_iterator iter = cuts.begin();
      iter != cuts.end(); ++iter) {
    std::cout << *iter << std::endl;
    cut_s = cut_s && TCut(iter->c_str());
  }
  //std::cout << cut_s << std::endl;
  std::cout << std::endl;
  
  std::cout << "Trainer: " << trainer << std::endl << std::endl;
  std::cout << "Classifiers:" << std::endl;
  for(std::vector<std::pair<std::string, std::string> >::const_iterator iter = classifiers.begin();
      iter != classifiers.end(); ++iter) {
    std::cout << iter->first << ": " << iter->second << std::endl;
  }
  std::cout << std::endl;

  // Check input file existence
  //TString pwd(getenv("LS_SUBCWD"));
  //TString ipath = pwd;
  //ipath += "/";
  //ipath += inputfileName;
  TString ipath = inputfileName;
  if(gSystem->AccessPathName(ipath)) {
    std::cout << "Input file " << ipath << " does not exist!" << std::endl;
    return 1;
  }
  
  // Create output file
  TFile *outputFile = TFile::Open(outputfileName, "RECREATE");

  // Initialize factory
  TMVA::Factory *factory = new TMVA::Factory("trainTMVA", outputFile, "!V:!Silent:Color");

  // Assign variables
  for(std::vector<std::string>::const_iterator iter = variables.begin();
      iter != variables.end(); ++iter) {
    factory->AddVariable(*iter, 'F');
  }

  // Read input file
  TFile *inputFile = TFile::Open(ipath);
  
  // Read signal and background trees, assign weights
  TTree *signal     = dynamic_cast<TTree *>(inputFile->Get("TreeS"));
  TTree *background = dynamic_cast<TTree *>(inputFile->Get("TreeB"));
  
  // Register tree
  factory->AddSignalTree(signal, signalWeight);
  factory->AddBackgroundTree(background, backgroundWeight);

  // Prepare training and testing
  //factory->PrepareTrainingAndTestTree(cut_s, cut_b, "NSigTrain=4000:NBkgTrain=230000:SplitMode=Random:NormMode=NumEvents:!V");
  factory->PrepareTrainingAndTestTree(cut_s, cut_s, trainer);

  // Book MVA methods
  for(std::vector<std::pair<std::string, std::string> >::const_iterator iter = classifiers.begin();
      iter != classifiers.end(); ++iter) {
    TMVA::Types::EMVA type = getType(iter->first);
    factory->BookMethod(type, iter->first, iter->second);
  }

  // Train classifiers
  factory->TrainAllMethods();

  // Evaluate all classifiers
  factory->TestAllMethods();

  // Compare classifier performance
  factory->EvaluateAllMethods();

  // Save output
  outputFile->Close();

  std::cout << "Created output file " << outputfileName << std::endl;
  //std::cout << "Launch TMVA GUI by 'root -l TMVAGui.C'" << std::endl;

  // Clean up
  delete factory;
  inputFile->Close();
}

TMVA::Types::EMVA getType(std::string desc) {
  std::string type("");
  size_t t = desc.find("_");
  if(t < desc.length())
    type = desc.substr(0, t);
  else
    type = desc;

  if(type == "Cuts")
    return TMVA::Types::kCuts;
  else if(type == "Likelihood")
    return TMVA::Types::kLikelihood;
  else if(type == "PDERS")
    return TMVA::Types::kPDERS;
  else if(type == "HMatrix")
    return TMVA::Types::kHMatrix;
  else if(type == "Fisher")
    return TMVA::Types::kFisher;
  else if(type == "KNN")
    return TMVA::Types::kKNN;
  else if(type == "CFMlpANN")
    return TMVA::Types::kCFMlpANN;
  else if(type == "TMlpANN")
    return TMVA::Types::kTMlpANN;
  else if(type == "MLP")
    return TMVA::Types::kMLP;
  else if(type == "BDT")
    return TMVA::Types::kBDT;
  else if(type == "RuleFit")
    return TMVA::Types::kRuleFit;
  else if(type == "SVM")
    return TMVA::Types::kSVM;
  else if(type == "FDA")
    return TMVA::Types::kFDA;
  else {
    std::cout << "Unkown classifer type " << type << std::endl;
    exit(1);
  }
}

bool parseConf(std::string filename, std::vector<std::string>& variables,
               std::vector<std::string>& cuts, std::string& trainer,
               std::vector<std::pair<std::string, std::string> >& classifiers) {
  variables.clear();
  classifiers.clear();
  trainer = "";

  std::ifstream file(filename.c_str());
  if(!file) {
    std::cout << "File " << filename << " does not exist" << std::endl;
    return false;
  }

  std::string line;

  enum mode_t {kNone, kVar, kCut, kTrain, kClass};
  mode_t mode = kNone;
  int lineno = 0;

  while(! file.eof()) {
    std::getline(file, line);
    ++lineno;

    std::vector<std::string> parsed = preprocessLine(line);
    if(parsed.size() == 0) continue;

    if(line == "Variables:")
      mode = kVar;
    else if(line == "Cuts:")
      mode = kCut;
    else if(line == "Trainer:")
      mode = kTrain;
    else if(line == "Classifiers:")
      mode = kClass;
    else if(mode == kVar) {
      if(parsed.size() != 1) {
        std::cout << "Parse error at line " << lineno << ": \"" << line << "\"" << std::endl;
        std::cout << "Expected only one string at line" << std::endl;
        return false;
      }
      variables.push_back(parsed[0]);
    }
    else if(mode == kCut) {
      if(parsed.size() > 0) {
        std::string s(parsed[0]);
        for(std::vector<std::string>::const_iterator iter = parsed.begin()+1; iter != parsed.end(); ++iter) {
          s += " ";
          s += *iter;
        }
        cuts.push_back(s);
      }
    }
    else if(mode == kTrain) {
      if(parsed.size() != 1) {
        std::cout << "Parse error at line " << lineno << ": \"" << line << "\"" << std::endl;
        std::cout << "Expected only one string at line" << std::endl;
        return false;
      }
      trainer = parsed[0];
    }
    else if(mode == kClass) {
      if(parsed.size() != 2) {
        std::cout << "Parse error at line " << lineno << ": \"" << line << "\"" << std::endl;
        std::cout << "Expected two strings at line" << std::endl;
        return false;
      }
      classifiers.push_back(std::make_pair(parsed[0], parsed[1]));
    }
  }
  file.close();
  
  return true;
}

std::vector<std::string> preprocessLine(std::string& line) {
  std::vector<std::string> ret;

  std::string temp("");

  // preprocess comments
  size_t comment = line.find("//");
  if(comment < line.length())
    line = line.substr(0, comment);

  // split by whitespaces
  for(unsigned int i=0; i<line.length(); ++i) {
    if(std::isspace(line[i]) && temp.length() > 0) {
      ret.push_back(temp);
      temp = "";
    }
    else if(!std::isspace(line[i]))
      temp += line[i];
  }
  if(temp.length() > 0)
    ret.push_back(temp);

  return ret;
}
