#include "DQMOffline/Trigger/interface/HLTTauTrkDQMOfflineSource.h"
#include "Math/GenVector/VectorUtil.h"
#include <iostream>
#include <iomanip>
#include <fstream>

HLTTauTrkDQMOfflineSource::HLTTauTrkDQMOfflineSource(const edm::ParameterSet& iConfig){
   jetTagSrc_ = iConfig.getParameter<edm::InputTag>("ConeIsolation");
   jetMCTagSrc_ = iConfig.getParameter<edm::InputTag>("refCollection");
   caloJets_ = iConfig.getParameter<edm::InputTag>("InputJets");
   mcMatch_ = iConfig.getParameter<double>("MatchDeltaR");
   match_ = iConfig.getParameter<bool>("doReference");
   tT_ = iConfig.getParameter<std::string>("DQMFolder");
   outFile_ = iConfig.getParameter<std::string>("OutputFileName");
   EtMin_ = (iConfig.getParameter<double>("EtMin"));
   EtMax_ = (iConfig.getParameter<double>("EtMax"));
   NBins_ = (iConfig.getParameter<int>("NBins"));

   DQMStore* store = &*edm::Service<DQMStore>();
  
   if(store){		//Create the histograms
      
      store->setCurrentFolder(tT_);
      jetEt = store->book1D((tT_+"jetEt").c_str(), "jetEt",NBins_,EtMin_,EtMax_);
      jetEta = store->book1D((tT_+"jetEta").c_str(), "jetEta", 50, -2.5, 2.5);
      nL2EcalIsoJets = store->book1D((tT_+"nL2EcalIsoJets").c_str(), "nInputJets", 10, 0, 10);
      nL25Jets = store->book1D((tT_+"nL25Jets").c_str(), "nIsoJets", 10, 0, 10);
      nPxlTrksInL25Jet = store->book1D((tT_+"nPxlTrksInJet").c_str(), "nPxlTrksInJet", 30, 0, 30);
      nQPxlTrksInL25Jet = store->book1D((tT_+"nQPxlTrksInJet").c_str(),"nQPxlTrksInJet", 15, 0, 15);
      signalLeadTrkPt = store->book1D((tT_+"signalLeadTrkPt").c_str(), "signalLeadTrkPt", 75, 0, 150);
      l25IsoJetEt = store->book1D((tT_+"IsoJetEt").c_str(), "IsoJetEt", NBins_, EtMin_,EtMax_);
      l25IsoJetEta = store->book1D((tT_+"IsoJetEta").c_str(), "IsoJetEta", 50, -2.5, 2.5);

   }
}


HLTTauTrkDQMOfflineSource::~HLTTauTrkDQMOfflineSource(){
}



void 
HLTTauTrkDQMOfflineSource::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup){
   using namespace edm;
   using namespace reco;
   
   Handle<IsolatedTauTagInfoCollection> tauTagInfoHandle;
   if(iEvent.getByLabel(jetTagSrc_, tauTagInfoHandle))
     {
   
       Handle<LVColl> mcInfo;							 
       if(match_){ 								 
	 iEvent.getByLabel(jetMCTagSrc_, mcInfo);				 
       } 									 
   
       Handle<CaloJetCollection> caloJetHandle;			   
       if(iEvent.getByLabel(caloJets_, caloJetHandle))
	 {  	   
	   const CaloJetCollection & caloJets = *(caloJetHandle.product());
	   bool l2Match = 0;	
	   unsigned int cjIt;
	   for(cjIt = 0; cjIt != caloJets.size(); ++cjIt){
	     LV lVL2Calo = caloJets[cjIt].p4();
	     if(match_ )l2Match = match(lVL2Calo, *mcInfo);
	     else l2Match = 1;
      
	     bool l25Match = 0;
	     if(&(*tauTagInfoHandle)){
	       const IsolatedTauTagInfoCollection & tauTagInfoColl = *(tauTagInfoHandle.product());
	       IsolatedTauTagInfo tauTagInfo = tauTagInfoColl[cjIt];	    
	       LV theJet(tauTagInfo.jet()->px(), tauTagInfo.jet()->py(),
		     tauTagInfo.jet()->pz(),tauTagInfo.jet()->energy());  		         
         												         
	       if(match_)l25Match = match(theJet, *mcInfo); 						         
	       else l25Match = 1;												         
	   
	       if(l2Match&&l25Match){
		 jetEt->Fill(theJet.Et()); 		  							         
		 jetEta->Fill(theJet.Eta());		  						         
		 nL2EcalIsoJets->Fill(caloJets.size());
		 nL25Jets->Fill(tauTagInfoColl.size());											         
		 nPxlTrksInL25Jet->Fill(tauTagInfo.allTracks().size());								    
		 nQPxlTrksInL25Jet->Fill(tauTagInfo.selectedTracks().size());							    
	     
		 const TrackRef leadTrk = tauTagInfo.leadingSignalTrack();
		 if(!leadTrk) std::cout <<  "No leading track found " << std::endl;
		 else{
	       
		   signalLeadTrkPt->Fill(leadTrk->pt());				 

		   if(tauTagInfo.discriminator()==1){
		     l25IsoJetEta->Fill(theJet.Eta());
		     l25IsoJetEt->Fill(theJet.Et());
		   }
		 }
	       }
	     }
	 
	   }
	 }
     }   	
}											      	      



void HLTTauTrkDQMOfflineSource::beginJob(const edm::EventSetup&){

}


void HLTTauTrkDQMOfflineSource::endJob() {
   // Get the efficiency plots

   l25IsoJetEta->getTH1F()->Sumw2();
   jetEta->getTH1F()->Sumw2();
   
   l25IsoJetEt->getTH1F()->Sumw2();
   jetEt->getTH1F()->Sumw2();

      
   //Write file
   if(outFile_.size()>0 &&(&*edm::Service<DQMStore>())) edm::Service<DQMStore>()->save (outFile_);

}

bool HLTTauTrkDQMOfflineSource::match(const LV& jet, const LVColl& matchingObject){
   bool matched = 0;
   if(matchingObject.size() !=0 ){
      std::vector<LV>::const_iterator lvIt = matchingObject.begin();
      for(;lvIt != matchingObject.end(); ++lvIt){
         double deltaR = ROOT::Math::VectorUtil::DeltaR(jet, *lvIt);
	 if(deltaR < mcMatch_) matched = 1;
      }
   } 
   return matched;
}

