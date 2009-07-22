// fminer.cpp
// © 2008 by Andreas Maunz, andreas@maunz.de, jun 2008

/*
    This file is part of LibFminer (libfminer).

    LibFminer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    LibFminer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LibFminer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fminer.h"
#include "globals.h"


// 1. Constructors and Initializers

Fminer::Fminer() : init_mining_done(false) {
  if (!fm::instance_present) {
      fm::database = NULL; fm::statistics = NULL; fm::chisq = NULL; fm::result = NULL;
      Reset();
      Defaults();
      fm::instance_present=true;
      if (getenv("FMINER_LAZAR")) fm::do_yaml = false;
      if (getenv("FMINER_SMARTS")) fm::gsp_out = false; 
  }
  else {
    cerr << "Error! Cannot create more than 1 instance." << endl; 
    exit(1);
  }
}

Fminer::Fminer(int _type, unsigned int _minfreq) : init_mining_done(false) {
  if (!fm::instance_present) {
      fm::database = NULL; fm::statistics = NULL; fm::chisq = NULL; fm::result = NULL;
      Reset();
      Defaults();
      SetType(_type);
      SetMinfreq(_minfreq);
      fm::instance_present=true;
      if (getenv("FMINER_LAZAR")) fm::do_yaml = false;
      if (getenv("FMINER_SMARTS")) fm::gsp_out = false; 
  }
  else {
    cerr << "Error! Cannot create more than 1 instance." << endl; 
    exit(1);
  }

}

Fminer::Fminer(int _type, unsigned int _minfreq, float _chisq_val, bool _do_backbone) : init_mining_done(false) {
  if (!fm::instance_present) {
      fm::database = NULL; fm::statistics = NULL; fm::chisq = NULL; fm::result = NULL;
      Reset();
      Defaults();
      SetType(_type);
      SetMinfreq(_minfreq);
      SetChisqSig(_chisq_val);
      SetBackbone(_do_backbone);
      fm::instance_present=true;
      if (getenv("FMINER_LAZAR")) fm::do_yaml = false;
      if (getenv("FMINER_SMARTS")) fm::gsp_out = false; 

  }
  else {
    cerr << "Error! Cannot create more than 1 instance." << endl; 
    exit(1);
  }

}

Fminer::~Fminer() {
    if (fm::instance_present) {
        delete fm::database;
        delete fm::statistics; 
        delete fm::chisq; 
        delete fm::graphstate;
        delete fm::closelegoccurrences;
        delete fm::legoccurrences;

        fm::candidatelegsoccurrences.clear();
        fm::candidatecloselegsoccs.clear();
        fm::candidatecloselegsoccsused.clear();

        fm::instance_present=false;
    }
}

void Fminer::Reset() { 
    if (fm::instance_present) {
        delete fm::database;
        delete fm::statistics;
        delete fm::chisq;
        delete fm::graphstate;
        delete fm::closelegoccurrences;
        delete fm::legoccurrences;
    }
    fm::database = new Database();
    fm::statistics = new Statistics();
    fm::chisq = new ChisqConstraint(0.95);
    fm::graphstate = new GraphState();
    fm::closelegoccurrences = new CloseLegOccurrences();
    fm::legoccurrences = new LegOccurrences();

    fm::candidatelegsoccurrences.clear();
    fm::candidatecloselegsoccs.clear();
    fm::candidatecloselegsoccsused.clear();

    SetChisqActive(true); 
    fm::result = &r;
    comp_runner=0; 
    comp_no=0; 
    init_mining_done = false;
}

void Fminer::Defaults() {
    fm::minfreq = 2;
    fm::type = 2;
    fm::do_backbone = true;
    fm::adjust_ub = true;
    fm::do_pruning = true;
    fm::console_out = false;
    fm::aromatic = false;
    fm::refine_singles = false;
    fm::do_output=true;
    fm::bbrc_sep=false;
    fm::most_specific_trees_only=false;
    fm::line_nrs=false;

    fm::updated = true;
    fm::do_yaml=true;
    fm::gsp_out=true;
}


// 2. Getter methods

int Fminer::GetMinfreq(){return fm::minfreq;}
int Fminer::GetType(){return fm::type;}
bool Fminer::GetBackbone(){return fm::do_backbone;}
bool Fminer::GetDynamicUpperBound(){return fm::adjust_ub;}
bool Fminer::GetPruning() {return fm::do_pruning;}
bool Fminer::GetConsoleOut(){return fm::console_out;}
bool Fminer::GetAromatic() {return fm::aromatic;}
bool Fminer::GetRefineSingles() {return fm::refine_singles;}
bool Fminer::GetDoOutput() {return fm::do_output;}
bool Fminer::GetBbrcSep(){return fm::bbrc_sep;}
bool Fminer::GetMostSpecTreesOnly(){return fm::most_specific_trees_only;}
bool Fminer::GetChisqActive(){return fm::chisq->active;}
float Fminer::GetChisqSig(){return fm::chisq->sig;}
bool Fminer::GetLineNrs() {return fm::line_nrs;}



// 3. Setter methods

void Fminer::SetMinfreq(int val) {
    if (val < 1) { cerr << "Error! Invalid value '" << val << "' for parameter minfreq." << endl; exit(1); }
    if (val > 1 && GetRefineSingles()) { cerr << "Warning! Minimum frequency of '" << val << "' could not be set due to activated single refinement." << endl;}
    fm::minfreq = val;
}

void Fminer::SetType(int val) {
    if ((val != 1) && (val != 2)) { cerr << "Error! Invalid value '" << val << "' for parameter type." << endl; exit(1); }
    fm::type = val;
}

void Fminer::SetBackbone(bool val) {
    if (val && !GetChisqActive()) {
        cerr << "Warning! BBRC mining could not be enabled due to deactivated significance criterium." << endl;
    }
    else {  
        if (!val && GetDynamicUpperBound()) {
            cerr << "Notice: Disabling dynamic upper bound pruning due to switched-off BBRC mining." << endl;
            SetDynamicUpperBound(false);
        }
        fm::do_backbone = val;
    }
}

void Fminer::SetDynamicUpperBound(bool val) {
    if (val && !GetBackbone()) {
        cerr << "Warning! Dynamic upper bound pruning could not be enabled due to disabled BBRC mining." << endl;
    }
    else if (val && !GetChisqActive()) {
        cerr << "Warning! Dynamic upper bound pruning could not be enabled due to deactivated significance criterium." << endl;
    }
    else if (val && !GetPruning()) {
        cerr << "Warning! Dynamic upper bound pruning could not be enabled due to deactivated statistical metric pruning." << endl;
    }
    else {
        fm::adjust_ub=val; 
    }
}

void Fminer::SetPruning(bool val) {
    if (val && !GetChisqActive()) {
        cerr << "Warning! Statistical metric pruning could not be enabled due to deactivated significance criterium." << endl;
    }
    else {
        if (!val && GetDynamicUpperBound()) {
            cerr << "Notice: Disabling dynamic upper bound pruning." << endl;
            SetDynamicUpperBound(false); 
        }
        fm::do_pruning=val;
    }
}

void Fminer::SetConsoleOut(bool val) {
    if (val) {
        if (GetBbrcSep()) cerr << "Warning! Console output could not be enabled due to enabled BBRC separator." << endl;
        else fm::console_out=val;
    }
}

void Fminer::SetAromatic(bool val) {
    fm::aromatic = val;
}

void Fminer::SetRefineSingles(bool val) {
    fm::refine_singles = val;
    if (GetRefineSingles() && GetMinfreq() > 1) {
        cerr << "Notice: Using minimum frequency of 1 to refine singles." << endl;
        SetMinfreq(1);
    }
}

void Fminer::SetDoOutput(bool val) {
    fm::do_output = val;
}

void Fminer::SetBbrcSep(bool val) {
    fm::bbrc_sep=val;
    if (GetBbrcSep()) {
        if (GetConsoleOut()) {
             cerr << "Notice: Disabling console output, using result vector." << endl;
             SetConsoleOut(false);
        }
    }
}

void Fminer::SetMostSpecTreesOnly(bool val) {
    fm::most_specific_trees_only=val;
    if (GetMostSpecTreesOnly() && GetBackbone()) {
        cerr << "Notice: Disabling BBRC mining, getting most specific tree patterns instead." << endl;
        SetBackbone(false);
    }
}

void Fminer::SetChisqActive(bool val) {
    fm::chisq->active = val;
    if (!GetChisqActive()) {
        cerr << "Notice: Disabling dynamic upper bound pruning due to deactivated significance criterium." << endl;
        SetDynamicUpperBound(false); //order important
        cerr << "Notice: Disabling BBRC mining due to deactivated significance criterium." << endl;
        SetBackbone(false);
        cerr << "Notice: Disabling statistical metric pruning due to deactivated significance criterium." << endl;
        SetPruning(false);
    }
}

void Fminer::SetChisqSig(float _chisq_val) {
    if (_chisq_val < 0.0 || _chisq_val > 1.0) { cerr << "Error! Invalid value '" << _chisq_val << "' for parameter chisq." << endl; exit(1); }
    fm::chisq->sig = gsl_cdf_chisq_Pinv(_chisq_val, 1);
}

void Fminer::SetLineNrs(bool val) {
    fm::line_nrs = val;
}


// 4. Other methods

vector<string>* Fminer::MineRoot(unsigned int j) {
    fm::result->clear();
    if (!init_mining_done) {
        if (fm::chisq->active) {
            each (fm::database->trees) {
                if (fm::database->trees[i]->activity == -1) {
                    cerr << "Error! ID " << fm::database->trees[i]->orig_tid << " is missing activity information." << endl;
                    exit(1);
                }
            }
        }
        fm::database->edgecount (); 
        fm::database->reorder (); 
        initLegStatics (); 
        fm::graphstate->init (); 
        if (fm::bbrc_sep && !fm::do_backbone && fm::do_output && !fm::console_out) (*fm::result) << fm::graphstate->sep();
        init_mining_done=true; 
        cerr << "Settings:" << endl \
             << "---" << endl \
             << "Chi-square active (p-value): " << GetChisqActive() << " (" << GetChisqSig()<< ")" << endl \
             << "BBRC mining: " << GetBackbone() << endl \
             << "statistical metric (dynamic upper bound) pruning: " << GetPruning() << " (" << GetDynamicUpperBound() << ")" << endl \
             << "---" << endl \
             << "Minimum frequency: " << GetMinfreq() << endl \
             << "Refine patterns with single support: " << GetRefineSingles() << endl \
             << "Most specific BBRC members: " << GetMostSpecTreesOnly() << endl \
             << "---" << endl;
    }
    if (j >= fm::database->nodelabels.size()) { cerr << "Error! Root node does not exist." << endl;  exit(1); }
    if ( fm::database->nodelabels[j].frequency >= fm::minfreq && fm::database->nodelabels[j].frequentedgelabels.size () ) {
        Path path(j);
        path.expand(); // mining step
    }
    return fm::result;
}

void Fminer::ReadGsp(FILE* gsp){
    fm::database->readGsp(gsp);
}

bool Fminer::AddCompound(string smiles, unsigned int comp_id) {
    bool insert_done=false;
    if (comp_id<=0) { cerr << "Error! IDs must be of type: Int > 0." << endl;}
    else {
        if (fm::database->readTreeSmi (smiles, comp_no, comp_id, comp_runner)) {
            insert_done=true;
            comp_no++;
        }
        else { cerr << "Error on compound " << comp_runner << ", id " << comp_id << "." << endl; }
        comp_runner++;
    }
    return insert_done;
}

bool Fminer::AddActivity(bool act, unsigned int comp_id) {
    if (fm::database->trees_map[comp_id] == NULL) { 
        cerr << "No structure for ID " << comp_id << ". Ignoring entry!" << endl; return false; 
    }
    else {
        if ((fm::database->trees_map[comp_id]->activity = act)) AddChiSqNa();
        else AddChiSqNi();
        return true;
    }
}

