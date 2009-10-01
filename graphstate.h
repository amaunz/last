// graphstate.h
// © 2008 by Andreas Maunz, andreas@maunz.de, jul 2008
// Siegfried Nijssen, snijssen@liacs.nl, feb 2004.

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

#ifndef GRAPHSTATE_H
#define GRAPHSTATE_H
#include <vector>
#include <iostream>
#include <algorithm>

#include "misc.h"
#include "database.h"
#include "closeleg.h"
#include "patterntree.h"

using namespace std;

typedef unsigned int Mark;

class GSWalk;

class GraphState {
  public:

    struct GSDeletedEdge {
      NodeId tonode, fromnode;
      EdgeLabel edgelabel;
      int postonode, posfromnode;
      bool close;
      Mark cyclemark;
    };

    vector<GSDeletedEdge> deletededges;
    // the current pattern
    vector<Tuple> *treetuples;
    vector<CloseTuple> *closetuples;
    vector<NodeId> nodesinpreorder;

    int backbonelength; // the length of the backbone, in number of nodes
    int startsecondpath; // the position of the second part of the backbone in
                         // the treetuples.
    bool nasty;  // nasty == A-B-A-B-A-B -like cases
    NodeLabel centerlabel;
    EdgeLabel bicenterlabel;
    int closecount;
    bool selfdone; // set by isnormal to store whether the original graph has been
                   // normal-checked; we try to delay this until the very last moment,
                   // as we know that on this graph the normalisation procedure will
                   // have to go through all levels
    
    struct GSEdge {
      int tonode;
      int postonode; // position in the adjacency list of the corresponding reverse edge
      EdgeLabel edgelabel;
      Mark cyclemark;
      bool close; // closing edge
      GSEdge ():cyclemark ( 0 ), close ( false ) { }
      GSEdge ( int tonode, int postonode, EdgeLabel edgelabel, bool close = false ): tonode ( tonode ), postonode ( postonode ), edgelabel ( edgelabel ), cyclemark ( 0 ), close ( close ) { }
    };

    struct GSNode {
      NodeLabel label;
      short unsigned int maxdegree;
      vector<GSEdge> edges;
    };

    //keep for debugging purposes
    void makeState ( DatabaseTree *databasetree );
    void undoState ();
    void insertNode ( NodeLabel nodelabel, short unsigned int maxdegree );
    void deleteNode2 ();
    vector<GSNode> nodes;
    int edgessize;
    short unsigned int getNodeDegree ( int i ) const { return nodes[i].edges.size (); }
    short unsigned int getNodeMaxDegree ( int i ) const { return nodes[i].maxdegree; }
    GraphState  ();
    void determineCycles ( unsigned int usedbit );
    int enumerateSpanning ();
    int is_normal ();
    int normalizetree ();
    int normalizeSelf ();
    void init ();
    void insertStartNode ( NodeLabel nodelabel );
    void deleteStartNode ();
    void insertNode ( int from, EdgeLabel edgelabel, short unsigned int maxdegree );
    void deleteNode ();
    void insertEdge ( int from, int to, EdgeLabel edgelabel );
    void deleteEdge ( int from, int to );
    void deleteEdge ( GSEdge &edge ); // pushes deleted edge on stack
    void reinsertEdge (); // reinserts last edge on the stack
    NodeId lastNode () const { return nodes.size () - 1; }

    void print ( GSWalk* gsw); 

    void print ( FILE *f );
    void DfsOut(int cur_n, int from_n);
    void to_s ( string& oss );

    void print ( unsigned int frequency );
    void DfsOut(int cur_n, string& oss, int from_n);
    string to_s ( unsigned int frequency );
    string sep();

    void puti(FILE* f, int i);
};

struct GSWNode {
    //      v    <labs>         <occurrences active>    <occurrences inactive>
    // e.g. v    <6 7>          {0->2 1->3}             {2 3}
    // meaning                  T. 0 covered by 2 f.
    //                          on this node 
    set<InputNodeLabel> labs;
    map<Tid, int> a;
    map<Tid, int> i;
    int merge(GSWNode n);
    friend ostream& operator<< (ostream &out, GSWNode* n);
};

struct GSWEdge {
    //      e    <to>    <labs>          <occurrences active>    <occurrences inactive>
    // e.g. e    1       <2 1>           {0->2 1->3}             {2 3}
    // meaning                           T. 0 covered by 2 f.
    //                                   on this edge
    int to;
    set<InputEdgeLabel> labs;
    map<Tid, int> a;
    map<Tid, int> i;
    int merge(GSWEdge e);
    static bool lt_to (GSWEdge& e1, GSWEdge& e2){
        if (e1.to < e2.to) return 1;
        return 0;
    }
    static bool equal (GSWEdge* e1, GSWEdge* e2){
        if ((e1->to == e2->to) && std::equal(e1->labs.begin(), e1->labs.end(), e2->labs.begin())) return 1;
        return 0;
    }
    friend ostream& operator<< (ostream &out, GSWEdge* e);
};

class GSWalk {
  public:
      typedef vector<GSWNode> nodevector;      // position represents id, ids are always contiguous
      typedef map<int, map<int, GSWEdge> > edgemap; // key is id of from-node (and to-node)
      nodevector nodewalk;
      edgemap edgewalk;
      nodevector temp_nodewalk;
      edgemap temp_edgewalk;

      int cd (vector<int> core_ids, GSWalk* s);
      int merge (GSWalk* single, vector<int> core_ids);

      void add_edge(int f, GSWEdge e, GSWNode n, bool reorder);
      void up_edge(int i);
      static bool lt_to_map (pair<int, GSWEdge> a, pair<int, GSWEdge> b) {
        if (a.first < b.first) return 1;
        return 0;
      }
      friend ostream& operator<< (ostream &out, GSWalk* gsw);

};

#endif
