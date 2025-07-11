// @(#)root/tmva/pymva $Id$
// Author: Stefan Wunsch

/**********************************************************************************
 * Project: TMVA - a Root-integrated toolkit for multivariate data analysis       *
 * Package: TMVA                                                                  *
 * Class  : MethodPyKeras                                                         *
 *                                             *
 *                                                                                *
 * Description:                                                                   *
 *      Interface for Keras python package which is a wrapper for the Theano and  *
 *      Tensorflow libraries                                                      *
 *                                                                                *
 * Authors (alphabetical):                                                        *
 *      Stefan Wunsch <stefan.wunsch@cern.ch> - KIT, Germany                      *
 *                                                                                *
 * Copyright (c) 2016:                                                            *
 *      CERN, Switzerland                                                         *
 *      KIT, Germany                                                              *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted according to the terms listed in LICENSE           *
 * (see tmva/doc/LICENSE)                                          *
 **********************************************************************************/

#ifndef ROOT_TMVA_MethodPyKeras
#define ROOT_TMVA_MethodPyKeras

#include "TMVA/PyMethodBase.h"
#include <vector>

namespace TMVA {

   class MethodPyKeras : public PyMethodBase {

   public :

      // constructors
      MethodPyKeras(const TString &jobName,
            const TString &methodTitle,
            DataSetInfo &dsi,
            const TString &theOption = "");
      MethodPyKeras(DataSetInfo &dsi,
            const TString &theWeightFile);
      ~MethodPyKeras();

      void Train() override;
      void Init() override;
      void DeclareOptions() override;
      void ProcessOptions() override;

      // Check whether the given analysis type (regression, classification, ...)
      // is supported by this method
      Bool_t HasAnalysisType(Types::EAnalysisType type, UInt_t numberClasses, UInt_t) override;
      // Get signal probability of given event
      Double_t GetMvaValue(Double_t *errLower, Double_t *errUpper) override;
      std::vector<Double_t> GetMvaValues(Long64_t firstEvt, Long64_t lastEvt, Bool_t logProgress) override;
      // Get regression values of given event
      std::vector<Float_t>& GetRegressionValues() override;
      // Get class probabilities of given event
      std::vector<Float_t>& GetMulticlassValues() override;

      const Ranking *CreateRanking() override { return nullptr; }
      void TestClassification() override;
      void AddWeightsXMLTo(void*) const override{}
      void ReadWeightsFromXML(void*) override{}
      void ReadWeightsFromStream(std::istream&) override {} // backward compatibility
      void ReadWeightsFromStream(TFile&) override{} // backward compatibility
      void ReadModelFromFile() override;

      void GetHelpMessage() const override;

      /// enumeration defining the used Keras backend
      enum EBackendType { kUndefined = -1, kTensorFlow = 0, kTheano = 1, kCNTK = 2 };

      /// Get the Keras backend (can be: TensorFlow, Theano or CNTK)
      EBackendType GetKerasBackend();
      TString GetKerasBackendName();
      // flag to indicate we are using the Keras shipped with Tensorflow 2
      Bool_t UseTFKeras() const { return fUseTFKeras; }

   private:

      TString fFilenameModel; // Filename of the previously exported Keras model
      UInt_t fBatchSize {0}; // Training batch size
      UInt_t fNumEpochs {0}; // Number of training epochs
      Int_t fNumThreads {0}; // Number of CPU threads (if 0 uses default values)
      Int_t fVerbose; // Keras verbosity during training
      Bool_t fUseTFKeras { true};   // use Keras from Tensorflow default is true
      Bool_t fContinueTraining; // Load weights from previous training
      Bool_t fSaveBestOnly; // Store only weights with smallest validation loss
      Int_t fTriesEarlyStopping; // Stop training if validation loss is not decreasing for several epochs
      TString fLearningRateSchedule; // Set new learning rate at specific epochs
      TString fTensorBoard;          // Store log files during training
      TString fNumValidationString;  // option string defining the number of validation events
      TString fGpuOptions;    // GPU options (for Tensorflow to set in session_config.gpu_options)
      TString fUserCodeName; // filename of an optional user script that will be executed before loading the Keras model
      TString fKerasString;  // string identifying keras or tf.keras

      bool fModelIsSetup = false; // flag whether current model is setup for being used
      bool fModelIsSetupForEval = false; // flag to indicate whether model is setup for evaluation
      std::vector<float> fVals; // variables array used for GetMvaValue
      std::vector<float> fOutput; // probability or regression output array used for GetMvaValue
      UInt_t fNVars {0}; // number of variables
      UInt_t fNOutputs {0}; // number of outputs (classes or targets)
      TString fFilenameTrainedModel; // output filename for trained model

      void InitKeras();    //  initialize Keras (importing the readed modules)
      void SetupKerasModel(Bool_t loadTrainedModel); // setups the needed variables, loads the model
      void SetupKerasModelForEval();  // optimizes model for evaluation
      UInt_t  GetNumValidationSamples();  // get number of validation events according to given option

      ClassDefOverride(MethodPyKeras, 0);
   };

} // namespace TMVA

#endif // ROOT_TMVA_MethodPyKeras
