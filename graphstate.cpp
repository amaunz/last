// graphstate.cpp
// 
// © 2008 by Andreas Maunz, andreas@maunz.de, jul 2008
// Siegfried Nijssen, snijssen@liacs.nl, jan 2004.

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

#include <queue>
#include <sstream>

#include "graphstate.h"
#include "database.h"
#include "misc.h"

namespace fm {
    extern ChisqConstraint* chisq;
    extern bool console_out;
    extern bool gsp_out;
    extern bool do_yaml;
    extern Database* database;
    extern GraphState* graphstate;
    extern int die;
}

GraphState::GraphState () {
}

void GraphState::init () {
  edgessize = 0;
  closecount = 0;

  /*
  deletededges.clear();
  if (treetuples != NULL)  treetuples->clear();
  if (closetuples != NULL) closetuples->clear();
  nodesinpreorder.clear();
  */ 

  // 1 extra element on this stack in order to always be able to check the previous element
  vector_push_back ( GSDeletedEdge, deletededges, deletededge );
  deletededge.tonode = deletededge.fromnode = NONODE;
}

void GraphState::insertNode ( NodeLabel nodelabel, short unsigned int maxdegree ) {
  vector_push_back ( GSNode, nodes, node );
  node.label = nodelabel;
  node.maxdegree = maxdegree;
}

void GraphState::insertStartNode ( NodeLabel nodelabel ) {
  insertNode ( nodelabel, (short unsigned int) (-1) );
}

void GraphState::deleteNode2 () {
  nodes.pop_back ();
}

void GraphState::deleteStartNode () {
  deleteNode2 ();
}

void GraphState::insertNode ( int from, EdgeLabel edgelabel, short unsigned int maxdegree  ) {
  NodeLabel fromlabel = nodes[from].label, tolabel;
  DatabaseEdgeLabel &dataedgelabel = fm::database->edgelabels[fm::database->edgelabelsindexes[edgelabel]];
  if ( dataedgelabel.fromnodelabel == fromlabel )
    tolabel = dataedgelabel.tonodelabel;
  else
    tolabel = dataedgelabel.fromnodelabel;
  // insert into graph
  int to = nodes.size ();
  
  insertNode ( tolabel, maxdegree );
  nodes[to].edges.push_back ( GSEdge ( from, nodes[from].edges.size (), edgelabel ) );
  nodes[from].edges.push_back ( GSEdge ( to, nodes[to].edges.size () - 1, edgelabel ) );
  edgessize++;
}

void GraphState::deleteNode () {

  GSEdge &gsedge = nodes.back ().edges[0];
  int from = gsedge.tonode;
  nodes[from].edges.pop_back ();
  deleteNode2 ();
  edgessize--;
}

void GraphState::insertEdge ( int from, int to, EdgeLabel edgelabel ) {
  from--; to--;
  nodes[to].edges.push_back ( GSEdge ( from, nodes[from].edges.size (), edgelabel, true ) );
  nodes[from].edges.push_back ( GSEdge ( to, nodes[to].edges.size () - 1, edgelabel, true ) );
  edgessize++;
}

void GraphState::deleteEdge ( int from, int to ) {
  from--; to--;
  //EdgeLabel edgelabel = nodes[to].edges.back ().edgelabel;
  nodes[to].edges.pop_back ();
  nodes[from].edges.pop_back ();
  edgessize--;
}

void GraphState::deleteEdge ( GSEdge &edge ) {
  vector_push_back ( GSDeletedEdge, deletededges, deletededge );
  // fill in the info about the deleted edge
  deletededge.tonode = edge.tonode;
  deletededge.edgelabel = edge.edgelabel;
  deletededge.postonode = edge.postonode;
  deletededge.cyclemark = edge.cyclemark;
  deletededge.close = edge.close;
  GSEdge &edge2 = nodes[edge.tonode].edges[edge.postonode];
  deletededge.fromnode = edge2.tonode;
  deletededge.posfromnode = edge2.postonode;
  // remove the edge from the state
  if ( (int) nodes[deletededge.tonode].edges.size () != deletededge.postonode + 1 ) {
    // edge2 not the last
    GSEdge &edge3 = nodes[deletededge.tonode].edges.back ();
    nodes[edge3.tonode].edges[edge3.postonode].postonode = deletededge.postonode;
    swap ( edge2, edge3 );
  }
  
  nodes[deletededge.tonode].edges.pop_back ();
  
  if ( (int) nodes[deletededge.fromnode].edges.size () != deletededge.posfromnode + 1 ) {
    GSEdge &edge3 = nodes[deletededge.fromnode].edges.back ();
    nodes[edge3.tonode].edges[edge3.postonode].postonode = deletededge.posfromnode;
    swap ( edge, edge3 );
  }
  nodes[deletededge.fromnode].edges.pop_back ();
  edgessize--;
  closecount += deletededge.close;
}

void GraphState::reinsertEdge () {
  GSDeletedEdge &deletededge = deletededges.back ();
  vector<GSEdge> &edges1 = nodes[deletededge.tonode].edges;
  vector_push_back ( GSEdge, edges1, edge );
  edge.edgelabel = deletededge.edgelabel;
  edge.cyclemark = deletededge.cyclemark;
  edge.close = deletededge.close;
  edge.tonode = deletededge.fromnode;
  edge.postonode = deletededge.posfromnode;
  if ( deletededge.postonode != (int) edges1.size () - 1 ) {
     // reinsert at original location by swapping with the element at that position again
    GSEdge &edge3 = edges1[deletededge.postonode];
    nodes[edge3.tonode].edges[edge3.postonode].postonode = edges1.size () - 1;
    swap ( edge, edge3 );
  }
  vector<GSEdge> &edges2 = nodes[deletededge.fromnode].edges;
  vector_push_back ( GSEdge, edges2, edge2 );
  edge2.edgelabel = deletededge.edgelabel;
  edge2.tonode = deletededge.tonode;
  edge2.cyclemark = deletededge.cyclemark;
  edge2.close = deletededge.close;
  edge2.postonode = deletededge.postonode;
  if ( deletededge.posfromnode != (int)edges2.size () - 1 ) {
     // reinsert at original location by swapping with the element at that position again
    GSEdge &edge3 = edges2[deletededge.posfromnode];
    nodes[edge3.tonode].edges[edge3.postonode].postonode = edges2.size () - 1;
    swap ( edge2, edge3 );
  }
  deletededges.pop_back ();
  edgessize++;
  closecount -= deletededge.close;
}

// THE FUNCTIONS BELOW PERFORM GRAPH NORMALISATION.
// Given a graph, they determine whether the current code is canonical.
// The algorithm is a horror to implement.

int GraphState::normalizeSelf () {
  vector<pair<int, int> > removededges;
  selfdone = true;
  
  // temporarily change the situation to the original graph
  
  while ( deletededges.size () != 1 ) {
    removededges.push_back ( make_pair ( deletededges.back ().fromnode, deletededges.back ().posfromnode ) );
    reinsertEdge ();
  }
  
  for ( int i = closetuples->size () - 1; i >= 0; i-- ) 
    deleteEdge ( nodes[(*closetuples)[i].from-1].edges.back () );
  int b = normalizetree ();
  
  // then change the situation back
  
  for ( int i = closetuples->size () - 1; i >= 0; i-- )
    reinsertEdge ();
  
  while ( !removededges.empty () ) {
    deleteEdge ( nodes[removededges.back ().first].edges[removededges.back().second] );
    removededges.pop_back ();
  }
  
    
  return b;
}

// == 0 no lower found
// == 1 lower found, last tuple was however the only lower
// == 2 lower found, larger prefix was lower
int GraphState::is_normal () { 
  selfdone = false;
  
  int b = enumerateSpanning ();
  if ( b == 0 && !selfdone )
    b = normalizeSelf ();
  return b;
}

void GraphState::determineCycles ( unsigned int usedbit ) { 
  int nodestack[edgessize+1];
  int edgestack[edgessize+1];
  int stacktop = 1;
  nodestack[0] = edgestack[0] = -1; // to allow look at of array
  nodestack[1] = edgestack[1] = 0;
  bool instack[nodes.size ()];
  unsigned int deletebit = ~usedbit;
  
  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    instack[i] = false;
    vector<GSEdge> &edges = nodes[i].edges;
    for ( int j = 0; j < (int) edges.size (); j++ ) {
      edges[j].cyclemark &= deletebit;
    }
  }
  instack[0] = true;
  
  while ( stacktop > 0 ) {
    if ( (int) nodes[nodestack[stacktop]].edges.size () <= edgestack[stacktop] ) {
      instack[nodestack[stacktop]] = false;
      stacktop--;
      edgestack[stacktop]++;
      continue;
    }
    GSEdge &edge = nodes[nodestack[stacktop]].edges[edgestack[stacktop]];
    if ( edge.cyclemark & usedbit ) {
      edgestack[stacktop]++;
      continue;
    }
    if ( instack[edge.tonode] ) {
      if ( edge.tonode != nodestack[stacktop-1] ) {
        // otherwise walking back and forth
        int k = stacktop;
        bool had = true;
        while ( had ) {
          had = ( nodestack[k] != edge.tonode );
          GSEdge &edge2 = nodes[nodestack[k]].edges[edgestack[k]];
          GSEdge &edge3 = nodes[edge2.tonode].edges[edge2.postonode];
          edge2.cyclemark |= usedbit;
          edge3.cyclemark |= usedbit;
          k--;
        }
      }
      edgestack[stacktop]++;
    }
    else {
      stacktop++;
      nodestack[stacktop] = edge.tonode;
      instack[edge.tonode] = true;
      edgestack[stacktop] = 0;
    }
  }
}

// returns true if lower found, otherwise false
int GraphState::enumerateSpanning () {
  if ( edgessize == (int) nodes.size () - 1 ) {
    // we have a tree
    if ( closecount == (int) closetuples->size () )
      // in this case we have already considered this tree as a separate tree
      return 0;
    return normalizetree ();
  }
  else {
    unsigned int bit = 1 << ( deletededges.size () - 1 );
    determineCycles ( bit );
    for ( int i = 0; i < (int) nodes.size (); i++ ) {
      vector<GSEdge>& edges = nodes[i].edges;
      for ( int j = 0; j < (int) edges.size (); j++ ) {
        if ( ( edges[j].cyclemark & bit ) && 
             edges[j].tonode > i &&
             ( i < deletededges.back ().fromnode ||
             ( i == deletededges.back ().fromnode &&
               edges[j].tonode < deletededges.back ().tonode ) ) ) {
          deleteEdge ( edges[j] );
          int b = enumerateSpanning ();
          reinsertEdge ();
	  if ( b ) 
	    return b;
        }
      }
    }
  }  
  return 0;
}






// PRINT GSP TO STDOUT

void GraphState::print ( FILE *f ) {
  static int counter = 0;
  counter++;
  putc ( 't', f );
  putc ( ' ', f );
  puti ( f, (int) counter );
  putc ( '\n', f );
  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    putc ( 'v', f );
    putc ( ' ', f );
    puti ( f, (int) i );
    putc ( ' ', f );
    puti ( f, (int) fm::database->nodelabels[nodes[i].label].inputlabel );
    putc ( '\n', f );
  }
  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    for ( int j = 0; j < (int) nodes[i].edges.size (); j++ ) {
      GraphState::GSEdge &edge = nodes[i].edges[j];
      if ( i < edge.tonode ) {
        putc ( 'e', f );
        putc ( ' ', f );
        puti ( f, (int) i );
        putc ( ' ', f );
        puti ( f, (int) edge.tonode );
        putc ( ' ', f );
        puti ( f, (int) fm::database->edgelabels[
                 fm::database->edgelabelsindexes[edge.edgelabel]
                                                ].inputedgelabel );
        putc ( '\n', f );
      }
    }
  }
}


// GENERATE VECTOR REPRESENTATIONS FOR LATENT STRUCTURE MINING

void GraphState::print ( GSWalk* gsw, map<Tid, int> weightmap_a, map<Tid, int> weightmap_i ) {

  // convert occurrence lists to weight maps with initial weight 1
  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    set<InputNodeLabel> inl; inl.insert(fm::database->nodelabels[nodes[i].label].inputlabel);
    gsw->nodewalk.push_back( (GSWNode) { inl, weightmap_a, weightmap_i } );
  }

  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    for ( int j = 0; j < (int) nodes[i].edges.size (); j++ ) {
      GraphState::GSEdge &edge = nodes[i].edges[j];
      if ( i < edge.tonode ) {
          set<InputEdgeLabel> iel; iel.insert((InputEdgeLabel) fm::database->edgelabels[fm::database->edgelabelsindexes[edge.edgelabel]].inputedgelabel);
          gsw->edgewalk[i][edge.tonode] = (GSWEdge) { edge.tonode , iel, weightmap_a, weightmap_i } ;
      }
    }
  }

}


// PRINT SMARTS TO STDOUT

void GraphState::DfsOut(int cur_n, int from_n) {
    InputNodeLabel inl = fm::database->nodelabels[nodes[cur_n].label].inputlabel;
    if (inl!=254) {
        const char* str = etab.GetSymbol(inl);
        for(int i = 0; str[i] != '\0'; i++) putchar(str[i]);
    } else putchar('c'); // output nodelabel
    int fanout = (int) nodes[cur_n].edges.size ();
    InputEdgeLabel iel;
    for ( int j = 0; j < fanout; j++ ) {
        GraphState::GSEdge &edge = nodes[cur_n].edges[j];
        if ( edge.tonode != from_n) {
            if (fanout>2) putchar ('(');
            iel = fm::database->edgelabels[fm::database->edgelabelsindexes[edge.edgelabel]].inputedgelabel;
            switch (iel) {
            case 1:
                putchar('-');
                break;
            case 2:
                putchar('=');
                break;               
            case 3:
                putchar('#');
                break;
            case 4:
                putchar(':');
                break;
            default:
                cerr << "ERROR! Bond order of " << iel << " is not supported!" << endl;
                exit(1);
            }
            DfsOut(edge.tonode, cur_n);
            if (fanout>2) putchar(')');
        }
    }
}



// ENTRY: BRANCH TO GSP (STDOUT) or PRINT YAML/LAZAR TO STDOUT

void GraphState::print ( unsigned int frequency ) {
    if (!fm::chisq->active || fm::chisq->p >= fm::chisq->sig) {
        if (fm::gsp_out) { 
            print(stdout); 
        }
        else {
          if (fm::do_yaml) {
            putchar('-');
            putchar(' ');
            putchar('[');
            putchar(' ');
            putchar('"');
          }
          // output smarts 
          int i;
          for ( i = nodes.size()-1; i >= 0; i-- ) {   // edges
              if (nodes[i].edges.size()==1) break;
          }
          DfsOut(i, i);
          if (fm::do_yaml) {
            putchar('"');
            putchar(',');
            putchar(' ');
          }
          // output chisq
          if (fm::chisq->active) {
            if (fm::do_yaml) { printf("%.4f, ", fm::chisq->p); }
            else putchar('\t');
          }
          // output freq
          if (fm::chisq->active) {
              if (frequency != (fm::chisq->fa+fm::chisq->fi)) { cerr << "Error: wrong counts! " << frequency << "!=" << fm::chisq->fa + fm::chisq->fi << "(" << fm::chisq->fa << "+" << fm::chisq->fi << ")" << endl; }
          }
          else { 
              if (fm::do_yaml) { printf("%i", frequency); }
              else { printf("%i\t", frequency); };
          }
          // output occurrences
          if (fm::chisq->active) {
              putchar ('[');
              set<Tid>::iterator iter;
              if (fm::do_yaml) {
                  for (iter = fm::chisq->fa_set.begin(); iter != fm::chisq->fa_set.end(); iter++) {
                      if (iter != fm::chisq->fa_set.begin()) putchar (',');
                      putchar (' ');
                      printf("%i", (*iter)); 
                  }
                  putchar (']');
                  putchar (',');
                  putchar (' ');
                  putchar ('[');
              }
              if (fm::do_yaml) {
                  for (iter = fm::chisq->fi_set.begin(); iter != fm::chisq->fi_set.end(); iter++) {
                      if (iter != fm::chisq->fi_set.begin()) putchar (',');
                      printf(" %i", (*iter)); 
                  }
              }
              if (!fm::do_yaml) {
                  set<Tid> ids;
                  ids.insert(fm::chisq->fa_set.begin(), fm::chisq->fa_set.end());
                  ids.insert(fm::chisq->fi_set.begin(), fm::chisq->fi_set.end());
                  for (iter = ids.begin(); iter != ids.end(); iter++) {
                      putchar(' ');
                      printf("%i", (*iter)); 
                  }
              }
              putchar(' ');
              putchar(']');
          }
          if (fm::do_yaml) { 
            putchar(' ');
            putchar(']');
          }
          if(fm::console_out) putchar('\n');
       }
    }
}




// PRINT GSP TO OSS

void GraphState::to_s ( string& oss ) {
  static int counter = 0;
  counter++;
  oss.append( "t");
  oss.append( " ");
  char x[20]; 
  sprintf(x, "%i", counter);
  (oss.append( x)).append("\n");
  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    oss.append( "v");
    oss.append( " ");
    sprintf(x, "%i", i);
    oss.append( x);
    oss.append( " ");
    sprintf(x, "%i", fm::database->nodelabels[nodes[i].label].inputlabel);
    oss.append( x);
    oss.append( "\n");
  }
  for ( int i = 0; i < (int) nodes.size (); i++ ) {
    for ( int j = 0; j < (int) nodes[i].edges.size (); j++ ) {
      GraphState::GSEdge &edge = nodes[i].edges[j];
      if ( i < edge.tonode ) {
    oss.append( "e");
    oss.append( " ");
    sprintf(x, "%i", i);
    oss.append( x);
    oss.append( " ");
    sprintf(x, "%i", edge.tonode);
    oss.append(x);
    oss.append( " ");
    sprintf(x, "%i", (int) fm::database->edgelabels[fm::database->edgelabelsindexes[edge.edgelabel]].inputedgelabel);
    oss.append( x);
        oss.append( "\n");
      }
    }
  }
}


// PRINT SMARTS TO OSS

void GraphState::DfsOut(int cur_n, string& oss, int from_n) {
    InputNodeLabel inl = fm::database->nodelabels[nodes[cur_n].label].inputlabel;
    (inl!=254) ? oss.append( etab.GetSymbol(inl)) : oss.append("c"); // output nodelabel
    int fanout = (int) nodes[cur_n].edges.size ();
    InputEdgeLabel iel;
    for ( int j = 0; j < fanout; j++ ) {
        GraphState::GSEdge &edge = nodes[cur_n].edges[j];
        if ( edge.tonode != from_n) {
            if (fanout>2) oss.append ("(");
            iel = fm::database->edgelabels[fm::database->edgelabelsindexes[edge.edgelabel]].inputedgelabel;
            switch (iel) {
            case 1:
                oss.append("-");
                break;
            case 2:
                oss.append("=");
                break;               
            case 3:
                oss.append("#");
                break;
            case 4:
                oss.append(":");
                break;
            default:
                cerr << "Error! Bond order of " << iel << " is not supported. Aborting." << endl;
                exit(1);
            }
            DfsOut(edge.tonode, oss, cur_n);
            if (fanout>2) oss.append(")");
        }
    }
}


// ENTRY: BRANCH TO GSP (OSS) or PRINT YAML/LAZAR TO OSS

string GraphState::to_s ( unsigned int frequency ) {

    if (!fm::chisq->active || fm::chisq->p >= fm::chisq->sig) {

        string oss;

        if (fm::gsp_out) { 
            to_s(oss); 
            return oss;
        }

        else {
          if (fm::do_yaml) oss.append ("- [ ");

          // output smarts 
          if (fm::do_yaml) oss.append ("\"");
          int i;
          for ( i = nodes.size()-1; i >= 0; i-- ) {   // edges
              if (nodes[i].edges.size()==1) break;
          }
          DfsOut(i, oss, i);
          if (fm::do_yaml) oss.append ("\", ");
          else oss.append("\t");

          // output chisq
          if (fm::chisq->active) {
            if (fm::do_yaml) { char x[20]; sprintf(x,"%.4f", fm::chisq->p); (oss.append(x)).append(", "); }
          }

          // output freq
          if (fm::chisq->active) {
              if (frequency != (fm::chisq->fa+fm::chisq->fi)) { cerr << "Notice: Wrong counts for frequency " << frequency << " [!=" << fm::chisq->fa << "(fa)+" << fm::chisq->fi << "(fi)]." << endl; }
          }
          else { 
              char x[20]; sprintf(x,"%i", frequency); 
              oss.append(x);
          }

          // output occurrences
          if (fm::chisq->active) {
              oss.append ("[");

              set<Tid>::iterator iter;
              char x[20];

              if (fm::do_yaml) {
                  set<Tid>::iterator begin = fm::chisq->fa_set.begin();
                  set<Tid>::iterator end = fm::chisq->fa_set.end();
                  set<Tid>::iterator last = end; if (fm::chisq->fa_set.size()) last = --(fm::chisq->fa_set.end());

                  for (iter = begin; iter != end; iter++) {
                      if (iter != begin) oss.append (",");
                      oss.append (" ");
                      sprintf(x,"%i", (*iter)); oss.append (x);
                      if ((last != end) && (iter == last)) oss.append (" ");
                  }
                  oss.append ("], [");

                  begin = fm::chisq->fi_set.begin();
                  end = fm::chisq->fi_set.end();
                  last = end; if (fm::chisq->fi_set.size()) last = --(fm::chisq->fi_set.end());

                  for (iter = begin; iter != end; iter++) {
                      if (iter != begin) oss.append (",");
                      oss.append (" ");
                      sprintf(x,"%i", (*iter)); oss.append (x);
                      if ((last != end) && (iter == last)) oss.append (" ");
                  }
              }

              if (!fm::do_yaml) {
                  set<Tid> ids;
                  ids.insert(fm::chisq->fa_set.begin(), fm::chisq->fa_set.end());
                  ids.insert(fm::chisq->fi_set.begin(), fm::chisq->fi_set.end());
                  for (iter = ids.begin(); iter != ids.end(); iter++) {
                      sprintf(x,"%i", (*iter)); 
                      (oss.append (" ")).append(x);
                  }
              }
              if (!fm::do_yaml) oss.append (" ]");
              else oss.append ("]");
          }

          if (fm::do_yaml) oss.append (" ]");

          fm::console_out ? oss.append ("\n") : oss.append ("");


          return oss;
       }

    }
    else return "";
}
  
string GraphState::sep() {
    if (fm::gsp_out) return "#";
    else if (fm::do_yaml) return "---";
    else return " ";
}

void GraphState::undoState () {
  int s = nodes.size ();
  for ( int i = 1; i < s; i++ )
    deleteNode ();
  deleteStartNode ();
}

// In this function the real work is done. Currently it is one function (645 lines),
// as many arrays are reused. This choice was made because this setup is more
// efficient (but less readable, unfortunately).
int GraphState::normalizetree () {
  unsigned int nrnodes = nodes.size ();
  int distmarkers[nrnodes];
  int adjacentdones[nrnodes];
  int queue[nrnodes];
  int queuebegin = 0;
  int queueend = 0;
  for ( int i = 0; i < (int) nrnodes; i++ )
    distmarkers[i] = adjacentdones[i] = 0;
  int onecnt = 0;
  
  // discover the leafs
  for ( int i = 0; i < (int) nodes.size (); i++ )
    if ( nodes[i].edges.size () == 1 ) {
      onecnt++;
      distmarkers[i] = 1;
      NodeId adjacent = nodes[i].edges[0].tonode;
      adjacentdones[adjacent]++;
      if ( nodes[adjacent].edges.size () - adjacentdones[adjacent] == 1 ) {
        distmarkers[adjacent] = 2;
        queue[queueend] = adjacent;
	queueend++;
      }
    }

  NodeId tonode;
  bool done = true;
  
  if ( queueend + onecnt == (int) nodes.size () ) {// otherwise all nodes have been done already
  // walk from the leafs towards the center
    if ( queueend == 2 ) // bicentered tree
      queuebegin++; // queuebegin should be on the second of two nodes
  }
  else
    while ( done ) {
      GSNode &node = nodes[queue[queuebegin]];
      done = false;
      for ( int i = 0; i < (int) node.edges.size (); i++ ) {
        tonode = node.edges[i].tonode;
        adjacentdones[tonode]++;
        int diff = nodes[tonode].edges.size () - adjacentdones[tonode];
        done |= diff;
        if ( diff == 1 ) {
          distmarkers[tonode] = distmarkers[queue[queuebegin]] + 1;
          queue[queueend] = tonode;
	  queueend++;
        }
      }
      queuebegin++;
    }

  
  // discover the two canonical labeled paths
  bool bicenter = queuebegin && ( distmarkers[queue[queuebegin]] == distmarkers[queue[queuebegin-1]] );
  Depth maxdepth = distmarkers[queue[queuebegin]];
  int pathlength = maxdepth * 2;
  if ( !bicenter ) {
    maxdepth--;
    pathlength--;
    if ( pathlength < backbonelength ) {
      return 2;
    }
    if ( pathlength > backbonelength )
      return 0;
    NodeLabel rl =  nodes[queue[queuebegin]].label;
    if ( rl < centerlabel ) {
      return 2;
    }
    if ( rl > centerlabel )
      return 0;
  }
  else {
    if ( pathlength < backbonelength ) {
      return 2;
    }
    if ( pathlength > backbonelength ) {
      return 0;
    }
    GSNode &node = nodes[queue[queuebegin]];
    int i;
    for ( i = 0; node.edges[i].tonode != queue[queuebegin-1]; i++ );
    EdgeLabel rl = node.edges[i].edgelabel;
    if ( rl < bicenterlabel ) {
      return 2;
    }
    if ( rl > bicenterlabel )
      return 0;
  }
  int nodes[nrnodes];
  int *depthnodes[maxdepth + 1]; // we sometimes overshoot this fill in, allthough we do not use the type 
                                 // This array is used to find the nodes for each type of the spanning
				 // tree that we are currently considering
  int depthnodessizes[maxdepth + 1];
  int minlabelednodes[2][nrnodes];
  int minlabelednodessize[2] = { 0, 0 };
  int children[nrnodes];
  int nodewalk;
  EdgeLabel pathedgelabels[2][nrnodes+1];
  int pathedgelabelssize = 1;
  for ( int i = 0; i < (int) maxdepth + 1; i++ ) 
    depthnodessizes[i] = 0;
  
  int* nodes_firstchild[nrnodes];
  int nodes_nochildren[nrnodes];
  int nodes_walkchild[nrnodes];
  int nodes_parent[nrnodes];
  EdgeLabel nodes_edgelabel[nrnodes];
  int nodes_code[nrnodes];
  int nodes_treenr[nrnodes];
  int nodes_marker[nrnodes];
  int lowesttreenr=0;
  int lowestlabel, secondlowestlabel;
  
  for ( int i = 0; i < (int) nrnodes; i++ )
    nodes_marker[i] = 0;
    
  if ( bicenter ) {
    int nodeid1 = queue[queuebegin];
    int nodeid2 = queue[queuebegin-1];
    depthnodes[0] = nodes;
    depthnodes[0][0] = nodeid1;
    depthnodes[0][1] = nodeid2;
    nodes_nochildren[nodeid1] = nodes_nochildren[nodeid2] = 0;
    nodes_edgelabel[nodeid1] = nodes_edgelabel[nodeid2] = 0;
    nodes_parent[nodeid1] = nodes_parent[nodeid2] = NONODE;
    nodes_marker[nodeid1] = nodes_marker[nodeid2] = 1;
    nodes_treenr[nodeid1] = 0;
    nodes_treenr[nodeid2] = 1;
    depthnodessizes[0] = 2;
    nodewalk = 2;
    lowesttreenr = -1;
    pathedgelabels[0][0] = pathedgelabels[1][0] = 0; // doesn't matter
  }
  else {
    // in case of a single root, we add all children of that root
    // at depth 0 of the resulting forest, thus obtaining a similar
    // situation to the bicentered tree case where both centers
    // are taken as the roots of two trees in a forest.
    // However, we have to take special care here to fill in all required
    // data correctly.
    int nodeid = queue[queuebegin];
    nodes_parent[nodeid] = NONODE;
    lowestlabel = MAXEDGELABEL;
    secondlowestlabel = MAXEDGELABEL;
    GSNode &node = GraphState::nodes[nodeid];
    int edgessize = node.edges.size ();
    depthnodes[0] = nodes;
    depthnodessizes[0] = edgessize;
    nodewalk = edgessize;
    for ( int j = 0; j < edgessize; j++ ) {
      NodeId tonode = node.edges[j].tonode;
      int lab = node.edges[j].edgelabel;
      depthnodes[0][j] = tonode;
      nodes_edgelabel[tonode] = lab;
      nodes_parent[tonode] = NONODE; // stop artificially nodeid;
      nodes_nochildren[tonode] = 0;
      nodes_treenr[tonode] = j; // this is (almost) all that we do it for
      if ( distmarkers[tonode] == distmarkers[nodeid] - 1 )
  	if ( lab < lowestlabel ) {
	  secondlowestlabel = lowestlabel;
	  lowestlabel = lab;
	}
	else
	  if ( lab == lowestlabel )
	    secondlowestlabel = -1; // we encounter the lowest label for the second time,
	                            // disable second lowest
	  else
	    if ( lab < secondlowestlabel )
	      secondlowestlabel = lab;
    }
    for ( int j = 0; j < edgessize; j++ ) {
      NodeId tonode = node.edges[j].tonode;
      int lab = node.edges[j].edgelabel;
      if ( distmarkers[tonode] == distmarkers[nodeid] - 1 )
        if ( lab == lowestlabel ) {
	  lowesttreenr = j;
	  nodes_marker[tonode] = 1;
	}
	else
	  if ( lab == secondlowestlabel )
	    nodes_marker[tonode] = 2;
    }
    pathedgelabels[0][0] = lowestlabel;
    if ( secondlowestlabel == -1 ) {
      lowesttreenr = -1; // no one tree is the lowest now
      pathedgelabels[1][0] = lowestlabel; // doesn't matter
    }
    else
      pathedgelabels[1][0] = secondlowestlabel;
  }
  
  
  int lowestlabeltreenr=0;
  
  // walk through the tree from the root, determine the lowest path,
  // and fill in all datastructures that we require for the next pass
  for ( Depth depth = 0; depth < maxdepth; depth++ ) {
    lowestlabel = MAXEDGELABEL;
    secondlowestlabel = MAXEDGELABEL;
    depthnodes[depth+1] = nodes + nodewalk;
    bool difftreenrlowlab = 0;
    for ( int i = 0; i < depthnodessizes[depth]; i++ ) {
      NodeId nodeid = depthnodes[depth][i];
      GSNode &node = GraphState::nodes[nodeid];
      int edgessize = node.edges.size ();
      nodes_walkchild[nodeid] = 0;
      nodes_nochildren[nodeid] = 0;
      nodes_firstchild[nodeid] = children + nodewalk;
      
      for ( int j = 0; j < edgessize; j++ ) {
        NodeId tonode = node.edges[j].tonode;
        if ( distmarkers[tonode] < distmarkers[nodeid] ) {
	  EdgeLabel lab = node.edges[j].edgelabel;
          if ( distmarkers[tonode] == distmarkers[nodeid] - 1 )
	    if ( nodes_marker[nodeid] == 1 ) {
  	      if ( lab < lowestlabel ) {
                minlabelednodessize[0] = 1;
		minlabelednodes[0][0] = tonode;
  	        lowestlabel = lab;
		lowestlabeltreenr = nodes_treenr[nodeid];
		difftreenrlowlab = 1;
	      }
	      else {
	        if ( lab == lowestlabel ) {
	          minlabelednodes[0][minlabelednodessize[0]] = tonode;
		  minlabelednodessize[0]++;
		  difftreenrlowlab &= ( nodes_treenr[nodeid] == lowestlabeltreenr );
		}
              }
	    }
            else {
  	      if ( nodes_marker[nodeid] == 2 )
  	        if ( lab < secondlowestlabel ) {
                  minlabelednodessize[1] = 1;
		  minlabelednodes[1][0] = tonode;
  	          secondlowestlabel = lab;
	        }
	        else 
	          if ( lab == secondlowestlabel ) {
	            minlabelednodes[1][minlabelednodessize[1]] = tonode;
		    minlabelednodessize[1]++;
		  }
            }
	  depthnodes[depth + 1][depthnodessizes[depth+1]] = tonode;
	  depthnodessizes[depth + 1]++;
	  
	  nodes_treenr[tonode] = nodes_treenr[nodeid];
	  nodes_parent[tonode] = nodeid;
	  nodes_edgelabel[tonode] = lab;
	  
	  nodes_nochildren[nodeid]++;
	  nodewalk++;
	}
      }
    }
    pathedgelabels[0][pathedgelabelssize] = lowestlabel;

    for ( int i = 0; i < minlabelednodessize[0]; i++ )
      nodes_marker[minlabelednodes[0][i]] = 1;
    if ( lowesttreenr == -1 )
      if ( difftreenrlowlab ) {
        // we have found two trees which have differently labeled paths now, we have
	// perform much additional work to find the second best tree and mark those
	lowesttreenr = lowestlabeltreenr;
	secondlowestlabel = MAXEDGELABEL;
 	for ( int r = 0; r < depthnodessizes[depth+1]; r++ ) {
	  int nodeid = depthnodes[depth+1][r];
	  int parentid = nodes_parent[nodeid];
	  if ( nodes_marker[parentid] == 1 &&
	       nodes_marker[nodeid] != 1 &&
	       distmarkers[nodeid] == distmarkers[parentid] - 1 &&
               nodes_treenr[nodeid] != lowesttreenr &&
	       nodes_edgelabel[nodeid] < secondlowestlabel )
	    secondlowestlabel = nodes_edgelabel[nodeid];
	}
	for ( int r = 0; r < depthnodessizes[depth+1]; r++ ) {
	  int nodeid = depthnodes[depth+1][r];
	  int parentid = nodes_parent[nodeid];
	  if ( nodes_marker[parentid] == 1 &&
	       nodes_marker[nodeid] != 1 &&
               nodes_treenr[nodeid] != lowesttreenr &&
	       distmarkers[nodeid] == distmarkers[parentid] - 1 &&
	       nodes_edgelabel[nodeid] == secondlowestlabel )
	    nodes_marker[nodeid] = 2;
	}
	pathedgelabels[1][pathedgelabelssize] = secondlowestlabel;
      }
      else { 
        // all trees in the running remain the same
	pathedgelabels[1][pathedgelabelssize] = lowestlabel;
      }
    else {
      pathedgelabels[1][pathedgelabelssize] = secondlowestlabel;
      for ( int i = 0; i < minlabelednodessize[1]; i++ )
        nodes_marker[minlabelednodes[1][i]] = 2;
    }
    pathedgelabelssize++;
  }
  
  // here we have both paths
  if ( bicenter ) {
    for ( int i = 1; i < (int) maxdepth; i++ ) {
      if ( pathedgelabels[0][i] < (*treetuples)[i-1].label ) {
        return 2;
      }
      if ( pathedgelabels[0][i] > (*treetuples)[i-1].label )
        return 0;
    }
    for ( int i = 1; i < (int) maxdepth; i++ ) {
      if ( pathedgelabels[1][i] < (*treetuples)[startsecondpath + i-1].label ) {
        return 2;
      }
      if ( pathedgelabels[1][i] > (*treetuples)[startsecondpath + i-1].label )
        return 0;
    }
  }
  else {
    for ( int i = 0; i < (int) maxdepth; i++ ) {
      if ( pathedgelabels[0][i] < (*treetuples)[i].label ) {
        return 2;
      }
      if ( pathedgelabels[0][i] > (*treetuples)[i].label )
        return 0;
    }
    for ( int i = 0; i < (int) maxdepth; i++ ) {
      if ( pathedgelabels[1][i] < (*treetuples)[startsecondpath + i].label ) {
        return 2;
      }
      if ( pathedgelabels[1][i] > (*treetuples)[startsecondpath + i].label )
        return 0;
    }
  }
  // if path contains only the same label, is bicentered and there are two node types on the path (A-B-A-B-A-B)
  // we artificially fill in the nodes_edgelabel of the two root nodes, such that the lowest node
  // has the lowest label. 
  // when we come here, we know that the path here equals the path in the status, so we can use the 'nasty case'
  // bit of the status.
  
  if ( nasty ) {
    int nodeid1 = queue[queuebegin];
    int nodeid2 = queue[queuebegin-1];
    nodes_edgelabel[nodeid1] = GraphState::nodes[nodeid1].label; // in this way we force one end to be the first
    nodes_edgelabel[nodeid2] = GraphState::nodes[nodeid2].label;    
  }
  
  // DO IT HERE
  
  // here we have in minlabelednodes the leafs at maximal depth for the lowest path
  // next, we're going bottom-up through the tree
  bool equal[nrnodes];
  
  for ( Depth depth = maxdepth - 1; ; depth-- ) {
    // sort the nodes at that type using insertion sort
    int size = depthnodessizes[depth];
    int *dnodes = depthnodes[depth];
    equal[0] = false;
    for ( int i = 1; i < size; i++ ) {
      int k = i, cmp;
      int node1 = dnodes[k];
      int ismin1 = ( nodes_treenr[node1] != lowesttreenr ); // 1 == not in lowest tree
      int depthlabel = pathedgelabels[ismin1][depth];
      k--;
      do {
        int node2 = dnodes[k];
	int ismin2 = ( nodes_treenr[node2] != lowesttreenr ); // returns 0 or 1
	cmp = ismin2 - ismin1;
	if ( cmp == 0 ) {
          if ( nodes_edgelabel[node1] == depthlabel ) 
   	    if ( nodes_edgelabel[node2] != depthlabel ) 
	      cmp = +1;
	    else
	      cmp = 0;
	  else	
	    if ( nodes_edgelabel[node2] == depthlabel ) 
	      cmp = -1;
	    else
	      cmp = nodes_edgelabel[node2] - nodes_edgelabel[node1];
	  if ( cmp == 0 ) {
	    // order of children important
	    int mins = min ( nodes_nochildren[node1], nodes_nochildren[node2] );
  	    int l = 0;
	    while ( cmp == 0 && l < mins ) {
	      cmp = nodes_code[nodes_firstchild[node2][l]] - nodes_code[nodes_firstchild[node1][l]];
	      l++;
	    }
	    if ( cmp == 0 )
	      cmp = nodes_nochildren[node1] - nodes_nochildren[node2];
	  }
	}
	
	if ( cmp > 0 ) {
	  dnodes[k+1] = dnodes[k];
	  equal[k+1] = equal[k];
  	  k--;
	}
      }
      while ( k >= 0 && cmp > 0 );
      dnodes[k+1] = node1;
      equal[k+1] = ( cmp == 0 ) && k >= 0;
    } 
    
    // filled in the sort info
    if ( depth == 0 ) {
      int codec = -1;
      for ( int i = 0; i < size; i++ ) {
        if ( !equal[i] )
          codec++;
        nodes_code[depthnodes[0][i]] = codec;
      }
      break;
    }
    
    int codec = -1;
    for ( int i = 0; i < size; i++ ) {
      if ( !equal[i] )
        codec++;
      int nodeid = depthnodes[depth][i];
      int parentid = nodes_parent[nodeid];
      nodes_code[nodeid] = codec;
      nodes_firstchild[parentid][nodes_walkchild[parentid]] = nodeid;
      nodes_walkchild[parentid]++;
    }      
  }
  
  // print string, put in classes at the same time?
  int stack[nrnodes];
  int depths[nrnodes];
  int preordernumber[nrnodes];
  
  int stacksize = 0;
  for ( int r = depthnodessizes[0] - 1; r >= 0; r-- ) {
    NodeId node = depthnodes[0][r];
    stack[stacksize] = node;
    depths[stacksize] = 0;
    stacksize++;
  }
  int mod;
  int preorder = 0;
  if ( bicenter ) {
    mod = -1;
  }
  else {
    mod = 0;
    preordernumber[queue[queuebegin]] = 0; // that's the first in the pre-order numbering
    preorder++;
  }
  int codewalk = 0;
  int bicnt = 0;
  while ( stacksize != 0 ) {
    stacksize--;
    int nodeid = stack[stacksize];
    int depth = depths[stacksize];
    preordernumber[nodeid] = preorder; // fill in pre-order numbers that may be needed in the next type
    if ( !bicenter || depths[stacksize] ) {
      if ( bicnt == 1 && preorder > startsecondpath )
        return 2;
      int cmp = depths[stacksize] + mod - (*treetuples)[codewalk].depth;
      if ( cmp > 0 || 
           ( cmp == 0 && 
             nodes_edgelabel[nodeid] < (*treetuples)[codewalk].label
           ) 
         ) {
        return 2;
      }
      if ( cmp < 0 || 
           ( cmp == 0 && 
             nodes_edgelabel[nodeid] > (*treetuples)[codewalk].label
           ) 
         )
        return 0;
      codewalk++;
    }
    else {
      bicnt++;
      if ( preorder && preorder<= startsecondpath )
        return 0;
    }
    for ( int j = nodes_nochildren[nodeid] - 1; j >= 0; j-- ) {
      stack[stacksize] = nodes_firstchild[nodeid][j];
      depths[stacksize] = depth + 1;
      stacksize++;
    }
    preorder++;
  }
  
    
  if ( closecount == (int) closetuples->size () ) {
    nodesinpreorder.resize ( nrnodes );
    for ( int i = 0; i < (int) nrnodes; i++ ) {
      nodesinpreorder[preordernumber[i]] = i;
    }
  }
  if ( !selfdone ) {
    int b;
    if ( (b = (int) normalizeSelf ()) ) // another was found to be lower than the current,
                                // otherwise we have filled in the class node numbers now
      return b;
  }
  
  bool nodeclose[nrnodes];
  for ( int i = 0; i < (int) nrnodes; i++ )
    nodeclose[i] = false;
  
  for ( int i = 1; i < (int) deletededges.size (); i++ ) {
    NodeId ni = deletededges[i].fromnode;
    while ( ni != NONODE ) {
      nodeclose[ni] = true;
      ni = nodes_parent[ni];
    }
    ni = deletededges[i].tonode;
    while ( ni != NONODE ) {
      nodeclose[ni] = true;
      ni = nodes_parent[ni];
    }
  }
  
  int *siblingstack[nrnodes];
  int siblingstacksize[nrnodes];
  int btcode[2 * nrnodes];
  int btparent[2 * nrnodes];
  int btcodesize = 0;
  int nodesinbt[nrnodes];
  siblingstack[0] = depthnodes[0];
  siblingstacksize[0] = depthnodessizes[0];
  stacksize = 1;
  
  // fill in the "backtrack string".
  // this string contains all nodes for which different permutations have to be checked
  // in the next type.
    
  
  while ( stacksize ) {
    stacksize--;
    int nsize = siblingstacksize[stacksize];
    int *nodes = siblingstack[stacksize];
    for ( int i = 0; i < nsize; ) {
      if ( nodeclose[nodes[i]] ) {
        int j = i - 1;
        while ( j >= 0 && nodes_code[nodes[j]] == nodes_code[nodes[i]] )
          j--;
        j++;
        while ( j < nsize && nodes_code[nodes[j]] == nodes_code[nodes[i]] ) {
          NodeId node = nodes[j];
          NodeId parent = nodes_parent[node];
          if ( parent != NONODE ) {
            btcode[btcodesize] = preordernumber[node] - preordernumber[parent];
            btparent[btcodesize] = nodesinbt[parent];
          }
          else {
            btcode[btcodesize] = preordernumber[node];
            btparent[btcodesize] = NONODE;
          }
          nodesinbt[node] = btcodesize;
          if ( nodeclose[node] && nodes_nochildren[node]) {
            siblingstack[stacksize] = nodes_firstchild[node];
            siblingstacksize[stacksize] = nodes_nochildren[node];
            stacksize++;
          }
          btcodesize++;
          j++;
        }
        btcode[btcodesize] = -1;
        btcodesize++;
        i = j;
      }
      else
        i++;
    }
  }
  
  if ( !bicenter )
    nodesinbt[queue[queuebegin]] = NONODE;
    
  // walk through all permutations, for each determine the coding of the closings
  int permstack[btcodesize];
  CloseTuple closetuples[deletededges.size ()];
  stacksize = 1;
  permstack[0] = 0;
  while ( true ) {
    int stackpos = stacksize - 1;
    if ( stacksize < btcodesize && btcode[permstack[stackpos]] != -1 ) {
      swap ( btcode[stackpos], btcode[permstack[stackpos]] );
      if ( btcode[stacksize] == -1 && stacksize + 1 < btcodesize ) {
        permstack[stacksize + 1] = stacksize + 1;
        stacksize += 2; 
      }
      else {
        permstack[stacksize] = stacksize;
        stacksize++;
      }
    }
    else {
      if ( stacksize == btcodesize ) { 
        // process this permutation
        
        // first fill in the unordered closing sequence
        int ddsize = deletededges.size () - 1;
        for ( int i = 0; i < ddsize; i++ ) {
          closetuples[i].label = deletededges[i+1].edgelabel;
          int p = 0;
          int ni = nodesinbt[deletededges[i+1].fromnode];
          while ( ni != NONODE ) {
            p += btcode[ni];
            ni = btparent[ni];
          }
          closetuples[i].from = nodesinpreorder[p] + 1; // TAKE CARE OF THE +1
          p = 0;
          ni = nodesinbt[deletededges[i+1].tonode];
          while ( ni != NONODE ) {
            p += btcode[ni];
            ni = btparent[ni];
          }
          closetuples[i].to = nodesinpreorder[p] + 1;
          if ( closetuples[i].to > closetuples[i].from )
            swap ( closetuples[i].to, closetuples[i].from );
        }
        
        // then sort it, while comparing with the current one.
        // stop when we discover that we can do better
        // algorithm: bubblesort ( in stead of insertion sort)
        for ( int i = 1; i < ddsize; i++ ) {
          int j = ddsize - 1;
          while ( j >= i ) {
            if ( closetuples[j-1] > closetuples[j] )
              swap ( closetuples[j-1], closetuples[j] );
            j--;
          }
          if ( closetuples[i-1] < (*GraphState::closetuples)[i-1] ) {
            return 2;
          }
          if ( closetuples[i-1] > (*GraphState::closetuples)[i-1] )
            goto end2; // continue with next permutation
        }
        if ( closetuples[ddsize-1] < (*GraphState::closetuples)[ddsize-1] ) {
          return 1; // the last tuple was lower!
        }
      }
end2:
      stackpos--;
      stacksize--;
      if ( stacksize == 0 ) {
        break; // all permutations considered
      }
      if ( btcode[stackpos] == -1 ) {
        stacksize--;
	stackpos--;
      }
      swap ( btcode[permstack[stackpos]], btcode[stackpos] );
      permstack[stackpos]++;
    }
  }

  return 0;
}

void GraphState::puti ( FILE *f, int i ) { 
  char array[100]; 
  int k = 0; 
  do { 
    array[k] = ( i % 10 ) + '0'; 
    i /= 10; 
    k++; 
  } 
  while ( i != 0 ); 
  do { 
    k--; 
    putc ( array[k], f ); 
  } while ( k ); 
}

//! s-sided stack of two features by walking core ids
//  NOTE: s is intended to 'carry' the growing meta pattern
//  direction 0: s takes precedence
int GSWalk::conflict_resolution (vector<int> core_ids, GSWalk* s, bool direction, int ceiling) {

    // sanity check: core
    if (core_ids.size()>0) {

        // get refined edges from this
        set<int> d1;  // the set of edges going out of j in this
        set<int> d2;  // the set of edges going out of j in s
        set<int> i12; // the common edges for j (core and node conflict edges)
        set<int> c12_tmp;
        set<int> c12; // the common edges for j without the core (exactly the node conflict edges)
        set<int> d12; // the mutex edges for j (neither core nor node conflict edges)
        set<int> d21; // 
        set<int> u12; // the incremental union set over both d12 and d21 and j (includes mutex and node conflict edges)
        set<int> index_revisit; // index nodes to be revisited, due to out-of-bound indices in to-nodes 

        map<int, map<int, GSWNode> > ninsert21; 
        map<int, map<int, GSWEdge> > einsert21; 
        map<int, map<int, GSWNode> > ninsert12; 
        map<int, map<int, GSWEdge> > einsert12; 
        //  ^^^      ^^^    
        //  to       from
        set<int> c12_inc;

        sort(core_ids.begin(), core_ids.end()); 
        if (nodewalk.size() < core_ids.size()) { cerr << "ERROR! More core ids than nodes." << endl; exit(1); }
        int border=core_ids.back();
        // prepare basic structure: copy core nodes and connecting edges from this to s, if s is empty
        if (s->nodewalk.size() == 0) {
            #ifdef DEBUG
            if (fm::die) {
                cout << "Initializing new SW" << endl;
            }
            #endif
            for (vector<int>::iterator index = core_ids.begin(); index!=core_ids.end(); index++) {
                edgemap::iterator from = edgewalk.find(*index);
                if (from!=edgewalk.end()) {
                    map<int, GSWEdge>& e1i = from->second;
                    for (map<int, GSWEdge>::iterator to=e1i.begin(); to!=e1i.end(); to++) {
                        if (to->first <= border) {
                            map<Tid, int> weightmap_a; 
                            map<Tid, int> weightmap_i; 
                            set<InputNodeLabel> inl;
                            set<InputEdgeLabel> iel;
                            GSWNode n = { inl, weightmap_a, weightmap_i };
                            GSWEdge e = { to->first, iel, weightmap_a, weightmap_i };
                            s->add_edge(from->first, e, n, 0, &core_ids, &u12);
                        }
                    }
                }
            }
        } 
        
        bool did_ceiling=0;

        // Do ... while still edges unexpanded for core ids 
        do {

            #ifdef DEBUG
            if (fm::die) {
                cout << "-CR begin-" << endl;
                cout << this << endl;
                cout << s << endl;
                cout << "core: '";
                each(core_ids) cout << core_ids[i] << " ";
                cout << "'" << endl;
            }
            #endif
        
            ninsert21.clear();
            einsert21.clear();
            ninsert12.clear();
            einsert12.clear();
            c12_inc.clear();
            index_revisit.clear();

            #ifdef DEBUG
            if (fm::die) {
                cerr << "U12: '";
                each_it(u12, set<int>::iterator) cerr << *it << " "; 
                cerr << "'" <<endl;
            }
            #endif

            // Gather candidate edges for all core ids!
            for (int index = 0; index<core_ids.size(); index++) {
                int j = core_ids[index];
                d1.clear();  
                d2.clear();
                edgemap::iterator e1 = edgewalk.find(j);
                edgemap::iterator e2 = s->edgewalk.find(j);
                if ( e1!=edgewalk.end() ) {
                    // remember tos of this->j
                    for (map<int, GSWEdge>::iterator it = e1->second.begin(); it!=e1->second.end(); it++) d1.insert(it->first);
                }
                if ( e2!=s->edgewalk.end() ) {
                    // remember tos of s->j
                    for (map<int, GSWEdge>::iterator it = e2->second.begin(); it!=e2->second.end(); it++) d2.insert(it->first);
                }
                #ifdef DEBUG
                if (fm::die) {
                    cout << "j: " << j << ", W1: " << e1->second.size() << ", W2: " << e2->second.size() << endl;
                }
                #endif
                // calculate intersection and diff sets of tos
                i12.clear(); set_intersection(d1.begin(), d1.end(), d2.begin(), d2.end(), std::inserter(i12, i12.end()));                  // intersection (symmetric)
                c12_tmp.clear(); set_difference(i12.begin(), i12.end(), core_ids.begin(), core_ids.end(), std::inserter(c12_tmp, c12_tmp.end()));
                c12.clear(); set_difference(c12_tmp.begin(), c12_tmp.end(), u12.begin(), u12.end(), std::inserter(c12, c12.end()));        // intersection \ core_ids (symmetric)

                // REMOVE MORE IDS FROM C12 HERE?????
                each_it(c12, set<int>::iterator) {
                    if (*it<core_ids.back()) c12.erase(*it);
                }

                d12.clear(); set_difference(d1.begin(), d1.end(), i12.begin(), i12.end(), std::inserter(d12, d12.end()));                  // mutex set
                d21.clear(); set_difference(d2.begin(), d2.end(), i12.begin(), i12.end(), std::inserter(d21, d21.end()));
        
                // intersection \ core
                each_it(c12, set<int>::iterator) {
                    #ifdef DEBUG
                    if (fm::die) {
                        map<int, GSWEdge>& w2j = e2->second;
                        cout << "C12: " << j << "->" << w2j.find(*it)->first;
                        cout << " < "; 
                        set<InputEdgeLabel>& labs = w2j.find(*it)->second.labs;
                        for (set<InputEdgeLabel>::iterator it2=labs.begin(); it2!=labs.end(); it2++) {
                            cout << *it2 << " ";
                        }
                        cout << ">" << endl;
                    }
                    #endif
                    if (ceiling==0 || *it<ceiling) c12_inc.insert(*it);
                }
        
                // single edges
                each_it(d21, set<int>::iterator) {
                    if (ceiling==0 || *it<ceiling) {
                        if (*it <= nodewalk.size()) { // only insert in-bound edges
                            #ifdef DEBUG
                            if (fm::die) {
                                map<int, GSWEdge>& w2j = e2->second;
                                cout << "D21: " << j << "->" << w2j.find(*it)->first;
                                cout << " < "; 
                                set<InputEdgeLabel>& labs = w2j.find(*it)->second.labs;
                                for (set<InputEdgeLabel>::iterator it2=labs.begin(); it2!=labs.end(); it2++) {
                                    cout << *it2 << " ";
                                }
                                cout << ">" << endl;
                            }
                            #endif
                            map<Tid, int> weightmap_a; 
                            map<Tid, int> weightmap_i; 
                            set<InputNodeLabel> inl;
                            set<InputEdgeLabel> iel;
                            GSWNode n = { inl, weightmap_a, weightmap_i };
                            GSWEdge e = { *it, iel, weightmap_a, weightmap_i };
                            ninsert21[*it][j]=n;
                            einsert21[*it][j]=e;
                        }
                        else { // remember index for next round
                            #ifdef DEBUG
                            if (fm::die) cout << "Node-to-revisit: " << j << endl;
                            #endif
                            index_revisit.insert(j);
                        }
                    }
                }

                // nothing inserted, so no recalculation of d12 necessary

                // single edges
                each_it(d12, set<int>::iterator) {
                    if (ceiling==0 || *it<ceiling) {
                        if (*it <= s->nodewalk.size()) {
                            #ifdef DEBUG
                            if (fm::die) {
                                map<int, GSWEdge>& w1j = e1->second;
                                cout << "D12: " << j << "->" << w1j.find(*it)->first; // needs no check for end() by def of d12
                                cout << " < ";
                                set<InputEdgeLabel>& labs = w1j.find(*it)->second.labs;
                                for (set<InputEdgeLabel>::iterator it2=labs.begin(); it2!=labs.end(); it2++) {
                                    cout << *it2 << " ";
                                }
                                cout << ">" << endl;
                            }
                            #endif
                            map<Tid, int> weightmap_a; 
                            map<Tid, int> weightmap_i; 
                            set<InputNodeLabel> inl;
                            set<InputEdgeLabel> iel;
                            GSWNode n = { inl, weightmap_a, weightmap_i };
                            GSWEdge e = { *it, iel, weightmap_a, weightmap_i };
                            ninsert12[*it][j]=n;
                            einsert12[*it][j]=e;
                        }
                        else {
                            #ifdef DEBUG
                            if (fm::die) cout << "Node-to-revisit: " << j << endl;
                            #endif
                            index_revisit.insert(j);
                        }
                    }
                }
                
            } // end for core ids

            
            map<int, map<int, GSWEdge> >::iterator it21 = einsert21.begin();
            map<int, map<int, GSWEdge> >::iterator it12 = einsert12.begin();

            // Must recognize 'bags'
            bool do_ceiling=0;
            int next_to=0; 
            if (u12.size()) next_to= maxi((*(--u12.end()))+1, core_ids.back()+1);
            else next_to=core_ids.back()+1;
            int c=0;
            
            // Insert lowest conflict edge!
            for(set<int>::iterator it=c12_inc.begin(); it!=c12_inc.end(); it++) {
                if ( (it21 == einsert21.end()) || (*it < it21->first) ) {
                    if ( (it12 == einsert12.end()) || (*it < it12->first) ) {
                         if (*it>next_to) { 
                            do_ceiling=1; c=*it; 
                            #ifdef DEBUG 
                            cout << "1) NEXT TO < " << *it << endl;
                            #endif
                         }
                         else u12.insert(*it);
                    }
                }
            }
            if (u12.size()) next_to= maxi((*(--u12.end()))+1, core_ids.back()+1);


            // Decide which edge to insert, then do it!
            if ( it21 != einsert21.end() || it12 != einsert12.end() ) {
                bool insertion_done = 0;
                if (it21 != einsert21.end()) {
                    // equal: direction should be 0 (siblingwalk dominance), so we merge from left to right and from top to down (in this order)
                    if ( (it12 == einsert12.end()) || (it21->first < it12->first) || (!direction && (it21->first == it12->first)) ) { 
                        if (it21->second.size()>1) { cerr << "Error! More than one edge to the same node (21)." << endl; exit(1); }
                        // to node is out of range: re-insert index for next round
                        if (it21->first > nodewalk.size()) {
                            cerr << "Error! 21: to-node '" << it21->first << "' is out of bound." << endl; exit(1);
                        }
                        else {
                            if (it21->first>next_to) { // Enter ceiling mode to resolve bag
                                #ifdef DEBUG 
                                cout << "2) NEXT TO < " << it21->first << endl;
                                #endif
                                do_ceiling=1;
                                c=it21->first;
                            }
                            else {
                                add_edge(
                                    it21->second.begin()->first, 
                                    it21->second.begin()->second, 
                                    ninsert21[it21->first][it21->second.begin()->first],
                                    1,
                                    &core_ids,
                                    &u12
                                );
                                u12.insert(it21->first);
                            }
                        }
                        insertion_done = 1;
                    }
                }
                if (it12 != einsert12.end()) {
                    if ( (it21 == einsert21.end()) ||  (it12->first < it21->first) || (direction && (it21->first == it12->first)) ) {
                        if (it12->second.size()>1) { cerr << "Error! More than one edge to the same node (12)." << endl; exit(1); }
                        if (it12->first > s->nodewalk.size()) {
                            cerr << "Error! 12: to-node '" << it12->first << "' is out of bound." << endl; exit(1);
                        }
                        else {
                            if (it12->first>next_to) { // Enter ceiling mode to resolve bag
                                #ifdef DEBUG 
                                cout << "3) NEXT TO < " << it12->first << endl;
                                #endif
                                do_ceiling=1;
                                c=it12->first;
                            }
                            else {
                                s->add_edge(
                                    it12->second.begin()->first, 
                                    it12->second.begin()->second, 
                                    ninsert12[it12->first][it12->second.begin()->first],
                                    1,
                                    &core_ids,
                                    &u12
                                );
                                u12.insert(it12->first);
                            }
                        }
                        insertion_done = 1;
                    }
                }

                if (!insertion_done) { cerr << "Error! No insertion done. " << einsert21.size() << " " << einsert12.size() << endl; exit(1); } 
            }

            // bag handling
            if (do_ceiling) {
                #ifdef DEBUG
                if (fm::die) {
                    cout << endl << endl << endl << "STARTING CEILING MODE next_to: '" << next_to << "' c: '" << c << "'" << endl << endl << endl;
                }
                #endif
                conflict_resolution(vector<int> (u12.begin(),u12.end()), s, direction, c);
                for (int i=next_to; i<c; i++) {
                    u12.insert(i);
                }
                #ifdef DEBUG
                if (fm::die) {
                    cout << endl << endl << endl << "ENDING CEILING MODE next_to: '" << next_to << "' c: '" << c << "'" << endl << endl << endl;
                }
                #endif
                do_ceiling=0;
                //did_ceiling=1;
            }


            #ifdef DEBUG
            else {
                if (fm::die) {
                     cout << "Finished CR." << endl;
                }
            }
            #endif

            each_it(edgewalk, edgemap::iterator) {
                for(map<int, GSWEdge>::iterator it2=it->second.begin(); it2!=it->second.end(); it2++) {
                    if (it2->first >= nodewalk.size()) {
                        cout << "Error! Nodewalk contains not enough nodes. Index: " << it2->first << ", size: " <<  nodewalk.size() << endl;
                        cout << this << endl;
                    }
                }
            }
            each_it(s->edgewalk, edgemap::iterator) {
                for(map<int, GSWEdge>::iterator it2=it->second.begin(); it2!=it->second.end(); it2++) {
                    if (it2->first >= s->nodewalk.size()) {
                        cout << "Error! S-Nodewalk contains not enough nodes. Index: " << it2->first << ", size; " << s->nodewalk.size() << endl;
                        cout << s << endl;
                    }
                }
            }


        } while (einsert21.size() || einsert12.size()); // Finished all edges for core ids
        #ifdef DEBUG
        if (fm::die) {
             cout << "Left while loop·" << endl;
        }
        #endif
       

        if (!ceiling) {
            #ifdef DEBUG
            if (fm::die) {
                //cout << this << endl;
                //cout << s << endl;
                cout << "-stack-" << endl;
            }
            #endif

            each_it(edgewalk, edgemap::iterator) {
                for(map<int, GSWEdge>::iterator it2=it->second.begin(); it2!=it->second.end(); it2++) {
                    if (it2->first >= nodewalk.size()) {
                        cout << "Error! Nodewalk contains not enough nodes. Index: " << it2->first << ", size: " <<  nodewalk.size() << endl;
                        cout << this << endl;
                        exit(1);
                    }
                }
            }
            each_it(s->edgewalk, edgemap::iterator) {
                for(map<int, GSWEdge>::iterator it2=it->second.begin(); it2!=it->second.end(); it2++) {
                    if (it2->first >= s->nodewalk.size()) {
                        cout << "Error! S-Nodewalk contains not enough nodes. Index: " << it2->first << ", size; " << s->nodewalk.size() << endl;
                        cout << s << endl;
                        exit(1);
                    }
                }
            }

            // stack labels and activities to s
            // all core id nodes
            // all edges leaving core id nodes
            // this includes edges inside the core, as well as edges leaving the core
            s->stack(this, core_ids);
        }


        #ifdef DEBUG
        if (fm::die) {
            cout << this << endl;
            cout << s << endl;
            cout << "-CR end-" << endl;
        }
        #endif
   

        // calculate one step core ids 
        #ifdef DEBUG
        if (fm::die) if (index_revisit.size()) cout << "Revisiting '";
        #endif
        each_it(index_revisit, set<int>::iterator) { 
            #ifdef DEBUG
            if (fm::die) cout << *it  << " ";
            #endif
            u12.insert(*it); 
        }
         #ifdef DEBUG
        if (fm::die) if (index_revisit.size()) cout << "'." << endl;
        #endif

        if (!ceiling && did_ceiling) { cout << "DID CEILING" << endl; exit(1); }
        else if (u12.size()) conflict_resolution(vector<int> (u12.begin(),u12.end()), s, direction, ceiling);

        if (!ceiling) each_it(s->nodewalk, nodevector::iterator) {
            if (it->labs.size() == 0) {
                cerr << "Error! S-Labels left to fill." << endl; exit(1);
            }
        }

        /*
        each_it(nodewalk, nodevector::iterator) {
            if (it->labs.size() == 0) {
                cerr << "Error! Labels left to fill." << endl; exit(1);
            }
        }
        */

        return 0;
    }
    return 1;
}

//! stacks w to this on selected ids
//  all core id nodes
//  all edges leaving core id nodes
//  includes edges inside the core, as well as edges leaving the core
//
int GSWalk::stack (GSWalk* w, vector<int> core_ids) {
    // sanity check: core ids present in nodewalks
    vector<int> test_ids; for (int i=0;i<nodewalk.size();i++) { test_ids.push_back(i); }
    if (!(includes(test_ids.begin(),test_ids.end(),core_ids.begin(),core_ids.end()))) {
        cerr << "Error! Set-more core ids than nodes." << endl; 
        exit(1);
    }
    // nodes clean: do the actual node merging
    each_it(core_ids, vector<int>::iterator) {
        nodewalk[*it].stack(w->nodewalk[*it]);
    }
    // edge merging
    each_it(core_ids, vector<int>::iterator) {
        edgemap::iterator from = edgewalk.find(*it);
        if (from!=edgewalk.end())   {
            edgemap::iterator w_from=w->edgewalk.find(from->first);
            if (w_from == w->edgewalk.end()) {
                #ifdef DEBUG
                if (fm::die) cout << "Different 'from'-components (" << from->first << ")." << endl; 
                #endif
            }
            else {
                map<int, GSWEdge>& e = from->second;
                map<int, GSWEdge>& se = w_from->second;
                for (map<int, GSWEdge>::iterator to=e.begin(); to!=e.end(); to++) {
                    map<int, GSWEdge>::iterator w_to=se.find(to->first);
                    if (w_to == se.end()) {
                        #ifdef DEBUG
                        if (fm::die) cout << "Different 'to'-component (" << to->first << ")." << endl;
                        #endif
                    }
                    else to->second.stack(w_to->second);
                }
            }
        }
    }
}

//! stacks a node n
//
int GSWNode::stack (GSWNode n) {
    labs.insert(n.labs.begin(), n.labs.end());
    for (map<Tid,int>::iterator it=n.a.begin(); it!=n.a.end(); it++) {
        a[it->first] = a[it->first] + it->second;
    }
    for (map<Tid,int>::iterator it=n.i.begin(); it!=n.i.end(); it++) {
        i[it->first] = i[it->first] + it->second;
    }
    return 0;
}

//! stacks an edge e
//
int GSWEdge::stack (GSWEdge e) {
    labs.insert(e.labs.begin(), e.labs.end());
    for (map<Tid,int>::iterator it=e.a.begin(); it!=e.a.end(); it++) {
        a[it->first] = a[it->first] + it->second;
    }
    for (map<Tid,int>::iterator it=e.i.begin(); it!=e.i.end(); it++) {
        i[it->first] = i[it->first] + it->second;
    }
    return 0;
}

//! Adds a node refinement for edge e and node n.
//
void GSWalk::add_edge (int f, GSWEdge e, GSWNode n, bool reorder, vector<int>* core_ids, set<int>* u12) {

    #ifdef DEBUG
    if (fm::die) {
        cout << "e.to " << e.to << endl;
    }
    #endif
    
    // DEFUNCT: to-node in existing positions?
    vector<int>::iterator it_ex = find(to_nodes_ex.begin(), to_nodes_ex.end(), e.to);
    if (it_ex!=to_nodes_ex.end()) { 
        cerr << "Error! To in ex." << endl; exit(1);
    }

    // to-core-range?
    bool to_core_range=0;
    if ((e.to >= *(core_ids->begin())) && (e.to <= *(core_ids->end()-1))) { 
        to_core_range=1; 
        #ifdef DEBUG 
        if (fm::die) cout << "to in core range (" << e.to << ")." << endl; 
        #endif
    }
    if (reorder && find(core_ids->begin(), core_ids->end(), e.to) != core_ids->end()) { cerr << "Error! e.to is a core-id." << endl; exit(1); }

    if (reorder) { // 'hard' insertion: reorder edges by moving 1 up

        if (to_core_range) {
        /*
            // find current edge
            GSWEdge val; edgemap::iterator it;
            for (it=edgewalk.begin(); it!=edgewalk.end(); it++) {
                if (it->second.find(e.to) != it->second.end()) {
                    val = edgewalk[it->first][e.to]; break;
                }
            }
            // move edge away
            if (u12->size()) { val.to= (*(--(u12->end())))+1 ; }
            int coreback=(core_ids->back())+1; if (coreback>val.to) val.to=coreback;
            add_edge(it->first, val, nodewalk[e.to], 1, core_ids, u12 );
            edgewalk[it->first].erase(e.to);
        */
        }

        else {
            for (edgemap::iterator from = edgewalk.begin(); from != edgewalk.end(); from++) {
                map<int, GSWEdge>& to_map = from->second;
                #ifdef DEBUG
                if (fm::die) cout << endl << from->first << " to" << endl << "to_map size: " << to_map.size() << endl;
                #endif
                map<int, GSWEdge>::iterator to=to_map.end();
                to--;
                for (; to != to_map.begin(); ) {
                    // increase all to-values equal or higher by 1
                    #ifdef DEBUG
                    if (fm::die) cout << "to->first " << to->first << endl;
                    #endif
                    if ((to->first >= e.to) && ((find(core_ids->begin(), core_ids->end(), to->first) == core_ids->end()) || to_core_range)) {
                        GSWEdge val = to->second; val.to++; // correct the data ...
                        pair<map<int, GSWEdge>::reverse_iterator, bool> p = to_map.insert(make_pair(val.to,val)); // ... and insert new value
                        if (!p.second) { cerr << "Error! Replaced a value while moving down. This should never happen." << endl; exit(1); }
                        #ifdef DEBUG
                        if (fm::die) cout << "    " << val.to-1 << "->" << val.to << endl;
                        #endif
                        to_map.erase(to--);
                    }
                    else --to;
                }
                if (to == to_map.begin()) {
                    // increase all to-values equal or higher by 1 (last element)
                    #ifdef DEBUG
                    if (fm::die) cout << "to->first " << to->first << endl;
                    #endif
                    if ((to->first >= e.to) && ((find(core_ids->begin(), core_ids->end(), to->first) == core_ids->end()) || to_core_range)) {
                        GSWEdge val = to->second; val.to++; // correct the data ...
                        pair<map<int, GSWEdge>::reverse_iterator, bool> p = to_map.insert(make_pair(val.to,val)); // ... and insert new value
                        if (!p.second) { cerr << "Error! Replaced a value while moving down. This should never happen." << endl; exit(1); }
                        #ifdef DEBUG
                        if (fm::die) cout << "    " << val.to-1 << "->" << val.to << endl;
                        #endif
                        to_map.erase(to);
                    }
                }
            }
            // increase all from-values equal or higher by 1
            for (edgemap::reverse_iterator from = edgewalk.rbegin(); from != edgewalk.rend(); from++) {
                if (from->first >= e.to) {
                    int i = from->first;
                    edgewalk[i+1]=edgewalk[i];
                    edgewalk.erase(i);
                }
            }
        }
    }

    // insert the edge from-to into edgewalk
    pair<map<int, GSWEdge>::iterator, bool> from = edgewalk[f].insert(make_pair(e.to,e));
    if (!from.second) { cerr << "Error! Key exists while adding an edge. " << endl; exit(1); }

    // insert to into nodewalk
    if (reorder) {                                                // called for non-empty s
        if (e.to == nodewalk.size()) {                            // case 1: 'off'-insertion
            nodewalk.resize(e.to+1);
            nodewalk[e.to] = n;
        }
        else if (e.to > nodewalk.size()) { 
            cerr << "Error! e.to out of bound." << endl; exit(1);
        }
        else { 
            if (to_core_range) {
                nodewalk[e.to]=n;
            }
            else {
                nodewalk.insert(nodewalk.begin()+e.to, n);     // case 3: 'hard'-insertion
            }
        }
    }
    else {                                                         // called only for empty s
        if (e.to >= nodewalk.size()) nodewalk.resize(e.to+1);
        nodewalk[e.to] = n;
    }
}

void GSWalk::svd () {
   gsl_matrix* A = gsl_matrix_calloc (edgewalk.size(), edgewalk.size());
   gsl_matrix* U = gsl_matrix_calloc (edgewalk.size(), edgewalk.size());
   gsl_vector* S = gsl_vector_calloc (edgewalk.size());
   gsl_matrix* V = gsl_matrix_calloc (edgewalk.size(), edgewalk.size());



   gsl_matrix_free(A);
   gsl_matrix_free(U);
   gsl_vector_free(S);
   gsl_matrix_free(V);
}
