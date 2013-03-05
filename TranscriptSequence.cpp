#include<algorithm>
#include<ctime>
#include<fstream>
#include<sstream>

#include "TranscriptSequence.h"

#include "common.h"

// Number of times we randomly probe for old cache record.
#define WORST_SEARCH_N 10

TranscriptSequence::TranscriptSequence(){//{{{
   srand(time(NULL));
   M=0;
   cM=0;
   gotGeneNames=false;
   useCounter = 0;
}//}}}
TranscriptSequence::TranscriptSequence(string fileName){//{{{
   TranscriptSequence();
   readSequence(fileName);
}//}}}
bool TranscriptSequence::readSequence(string fileName){//{{{
   fastaF.open(fileName.c_str());
   if(!fastaF.is_open()){
      error("TranscriptSequence: problem reading transcript file.\n");
      return false;
   }
   trSeqInfoT newTr;
   newTr.lastUse=0;
   newTr.cache=-1;
   string trDesc,geneName;
   long pos;
   istringstream geneDesc;
   gotGeneNames = true;
   while(fastaF.good()){
      while((fastaF.peek()!='>')&&(fastaF.good()))
         fastaF.ignore(1000,'\n');
      if(! fastaF.good())break;
      // Read description line:
      getline(fastaF, trDesc, '\n');
      // look for gene name:
      pos=trDesc.find("gene:");
      if(pos!=(long)string::npos){
         geneDesc.clear();
         geneDesc.str(trDesc.substr(pos+5));
         geneDesc >> geneName;
         geneNames.push_back(geneName);
      }else{
         gotGeneNames = false;
      }
      // remember position:
      newTr.seek=fastaF.tellg();
      trs.push_back(newTr);
   }
   // Exit if there was an error while reading the file.
   if(fastaF.bad()){
      error("TranscriptSequence: problem reading file.\n");
      return false;
   }
   M = trs.size();
   // Allocate cache.
   cache.resize(min(M,(long)TRS_CACHE_MAX));
   cachedTrs.resize(min(M,(long)TRS_CACHE_MAX));
   // Clear eof flag from input stream.
   fastaF.clear();
   return true;
}//}}}
const string* TranscriptSequence::getTr(long tr){//{{{
   if((tr<0)||(tr>=M))return NULL;
   // Update last use info.
   trs[tr].lastUse = useCounter++;
   // Return pointer to the sequence in cache.
   return &cache[acquireSequence(tr)];
}//}}}
string TranscriptSequence::getSeq(long tr,long start,long l,bool doReverse){//{{{
   // Return empty string for unknown transcript.
   if((tr<0)||(tr>=M))return "";
   // Update last use info.
   trs[tr].lastUse = useCounter++;
   // Get position within cache.
   long trI = acquireSequence(tr);
   
   // If position is not within the sequence, return Ns.
   if(start>=(long)cache[trI].size())return string(l,'N');

   string ret;
   // Copy appropriate sequence, fill up the rest with Ns.
   if(start<0){
      ret.assign(-start,'N');
      ret+=cache[trI].substr(0,l+start);
   }else{
      ret = cache[trI].substr(start,l);
      if(((long)ret.size()) < l)ret.append(l-ret.size(), 'N');
   }

   if(!doReverse){
      return ret;
   }else{
      // For reverse return reversed string with complemented bases.
      reverse(ret.begin(),ret.end());
      for(long i=0;i<l;i++)
         if((ret[i]=='A')||(ret[i]=='a'))ret[i]='T';
         else if((ret[i]=='T')||(ret[i]=='t'))ret[i]='A';
         else if((ret[i]=='C')||(ret[i]=='c'))ret[i]='G';
         else if((ret[i]=='G')||(ret[i]=='g'))ret[i]='C';
      return ret;
   }
}//}}}
long TranscriptSequence::acquireSequence(long tr){//{{{
   // If the sequence is stored in cache then just return it's cache index.
   if(trs[tr].cache!=-1)return trs[tr].cache;
   long i,newP,j;
   // See if cache is full.
   if(cM<TRS_CACHE_MAX){
      // If cache limit not reached, just add new sequence.
      newP=cM;
      cM++;
   }else{
      // If cache is full, look at WORST_SEARCH_N positions and choose the one least used.
      newP=rand()%cM;
      for(i=0;i<WORST_SEARCH_N;i++){
         j=rand()%cM;
         if(trs[cachedTrs[newP]].lastUse > trs[cachedTrs[j]].lastUse)newP=j;
      }
      // "remove" the transcript from position newP from cache.
      trs[cachedTrs[newP]].cache=-1;
      cache[newP].clear();
   }
   // Set input stream to transcript's position.
   fastaF.seekg(trs[tr].seek);
   string seqLine;
   // Read line by line until reaching EOF or next header line '>'.
   while((fastaF.peek()!='>')&&( getline(fastaF,seqLine,'\n').good())){
      cache[newP]+=seqLine;
   }
   if(fastaF.bad()){
      error("TranscriptSequence: Failed reading transcript %ld\n",tr);
      return 0;
   }
   // Clear flags.
   fastaF.clear();
   // Update cache information.
   cachedTrs[newP]=tr;
   trs[tr].cache=newP;
   // Return transcripts index within cache.
   return newP;
}//}}}

