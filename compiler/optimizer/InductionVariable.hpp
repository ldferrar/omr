/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef INDUCTIONVAR_INCL
#define INDUCTIONVAR_INCL

#include <stddef.h>                              // for NULL
#include <stdint.h>                              // for int32_t, int64_t
#include <map>                                   // for std::map
#include <utility>                               // for std::pair
#include "codegen/FrontEnd.hpp"                  // for TR_FrontEnd
#include "compile/Compilation.hpp"               // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"                      // for TR_Memory, etc
#include "env/jittypes.h"                        // for intptrj_t
#include "il/Block.hpp"                          // for Block
#include "il/DataTypes.hpp"                      // for DataTypes, etc
#include "il/ILOpCodes.hpp"                      // for ILOpCodes
#include "il/ILOps.hpp"                          // for ILOpCode, etc
#include "il/Node.hpp"                           // for Node, vcount_t
#include "il/Node_inlines.hpp"                   // for Node::getDataType
#include "il/Symbol.hpp"                         // for Symbol
#include "il/SymbolReference.hpp"                // for SymbolReference
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "infra/BitVector.hpp"                   // for TR_BitVector
#include "infra/List.hpp"                        // for List, etc
#include "optimizer/Optimization.hpp"            // for Optimization
#include "optimizer/OptimizationManager.hpp"     // for OptimizationManager
#include "optimizer/LoopCanonicalizer.hpp"  // for TR_LoopTransformer

class TR_BlockStructure;
class TR_Dominators;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class VPConstraint; }
namespace TR { class VPIntRange; }
namespace TR { class VPLongRange; }
namespace TR { class AutomaticSymbol; }
namespace TR { class NodeChecklist; }
namespace TR { class Optimizer; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

struct SymRefPair
   {
   union {
      TR::SymbolReference *_indexSymRef;
      int32_t _index;
      };
   bool _isConst;
   TR::SymbolReference *_derivedSymRef;
   SymRefPair *_next;
   };

// 64-bit
// sign-extension elimination

class TR_NodeIndexPair
   {
   public:

   TR_ALLOC(TR_Memory::InductionVariableAnalysis)

   TR_NodeIndexPair(TR::Node *node, int32_t index, TR_NodeIndexPair *next)
      : _node(node), _index(index), _next(next)
      {
      }

   TR::Node                    *_node;
   int32_t                    _index;
   TR_NodeIndexPair           *_next;
   };

class TR_StoreTreeInfo
   {
   public:

   TR_ALLOC(TR_Memory::InductionVariableAnalysis)

   TR_StoreTreeInfo(TR::TreeTop *tt, TR::Node *load, TR_NodeIndexPair *loads, TR::TreeTop *insertionTree, TR::Node *loadUsedInLoopIncrement, bool b, TR::Node *constNode, bool isAddition)
      : _tt(tt), _load(load), _loads(loads), _insertionTreeTop(insertionTree),  _loadUsedInLoopIncrement(loadUsedInLoopIncrement), _incrementInDifferentExtendedBlock(b), _constNode(constNode), _isAddition(isAddition)
      {
      }

   TR::TreeTop                 *_tt;
   TR::Node                    *_load;
   TR_NodeIndexPair           *_loads;
   TR::TreeTop                 *_insertionTreeTop;
   TR::Node                    *_loadUsedInLoopIncrement;
   bool                        _incrementInDifferentExtendedBlock;
   TR::Node                    *_constNode;
   bool                       _isAddition;
   };


/**
 * Class TR_LoopStrider
 * ====================
 *
 * The loop strider optimization creates derived induction variables 
 * (e.g. the address calculation for array accesses &a[i]) and 
 * increment/decrement the derived induction variables by the appropriate 
 * stride through every iteration of the loop. This can replace complex 
 * array address calculations inside the loop with a load (from register 
 * or memory). Note that we also create derived induction variables for 
 * non-internal pointers (like 4*i or 4*i+16) in cases when it is not 
 * possible to create internal pointers (e.g. a+4*i+16).
 */

class TR_LoopStrider : public TR_LoopTransformer
   {
   public:

   TR_LoopStrider(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopStrider(manager);
      }

   virtual int32_t perform();

   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t);

     //bool replaceAllInductionVariableComputations(TR::Block *loopInvariantBlock, TR_Structure *, TR::SymbolReference **, TR::SymbolReference *);
   virtual bool examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *, int32_t, TR::Node *, vcount_t, TR::SymbolReference **);
   virtual void updateStoreInfo(int32_t i, TR::TreeTop *tree);
   virtual void checkIfIncrementInDifferentExtendedBlock(TR::Block *block, int32_t inductionVariable);
   virtual bool isStoreInRequiredForm(int32_t symRefNum, TR_Structure *loopStructure);
   virtual TR::Node *updateLoadUsedInLoopIncrement(TR::Node *, int32_t);

   bool isStoreInRequiredForm(TR::Node *storeNode, int32_t symRefNum, TR_Structure *loopStructure);
   TR::Node *setUsesLoadUsedInLoopIncrement(TR::Node *node, int32_t k);

   TR::Node *getNewLoopIncrement(TR_StoreTreeInfo *info, int32_t k);
   TR::Node *getNewLoopIncrement(TR::Node *oldLoad, int32_t k, int32_t symRefNum);
   void addLoad(TR_StoreTreeInfo *info, TR::Node *load, int32_t index);
   void findOrCreateStoreInfo(TR::TreeTop *tree, int32_t i);

   int32_t findNewInductionVariable(TR::Node *, TR::SymbolReference **, bool, int32_t);
   TR::Node *isExpressionLinearInInductionVariable(TR::Node *, int32_t);
   bool isExpressionLinearInSomeInductionVariable(TR::Node *);
   bool identifyExpressionLinearInInductionVariable(TR::Node *, vcount_t);
   void identifyExpressionsLinearInInductionVariables(TR_Structure *, vcount_t);
   bool foundLoad(TR::TreeTop *, TR::Node *, int32_t, vcount_t);
   bool foundLoad(TR::TreeTop *, int32_t nextInductionVariableNumber, TR::Compilation *);
   bool foundValue(TR::Node *, int32_t, vcount_t);
   bool unchangedValueNeededIn(TR::Block *, int32_t, bool &seenStore);
   void changeLoopCondition(TR_BlockStructure *loopInvariantBlock, bool usingAladd, int32_t bestCandidate, TR::Node *storeOfDerivedInductionVariable);
   void createParmAutoPair(TR::SymbolReference *parmSymRef, TR::SymbolReference *autoSymRef);

   TR::Node *findReplacingNode(TR::Node *node, bool usingAladd, int32_t k);
   TR::DataType findDataType(TR::Node *node, bool usingAladd, bool isInternalPointer);
   void placeStore(TR::Node *newStore, TR::Block *loopInvariantBlock);
   void setInternalPointer(TR::Symbol *symbol, TR::AutomaticSymbol *pinningArrayPointer);
   void populateLinearEquation(TR::Node *node, int32_t loopDrivingInductionVar, int32_t derivedInductionVar, int32_t internalPointerSymbol, TR::Node *invariantMultiplicationTerm);

   // (64-bit)
   // sign-extension elimination

   // Maps are keyed by a node's global index, because a node's address could
   // be reused after its reference count decreases to zero.
   typedef TR::typed_allocator<std::pair<ncount_t, TR::Node*>, TR::Allocator> NodeMapAllocator;
   typedef std::map<ncount_t, TR::Node*, std::less<ncount_t>, NodeMapAllocator> NodeMap;

   void morphExpressionsLinearInInductionVariable(TR_Structure *, vcount_t);
   bool morphExpressionLinearInInductionVariable(TR::Node *, int32_t, TR::Node *, vcount_t);
   TR::Node *getInductionVariableNode(TR::Node *);
   void walkTreesAndFixUseDefs(TR_Structure *, TR::SymbolReference *, TR::NodeChecklist &);
   void replaceLoadsInStructure(TR_Structure *, int32_t, TR::Node *, TR::SymbolReference *, TR::NodeChecklist &, TR::NodeChecklist &);
   void replaceLoadsInSubtree(TR::Node *, int32_t, TR::Node *, TR::SymbolReference *, TR::NodeChecklist &, TR::NodeChecklist &);
   void widenComparison(TR::Node *, int32_t, TR::Node *, TR::NodeChecklist &);
   void eliminateSignExtensions(TR::NodeChecklist &);
   void eliminateSignExtensionsInSubtree(TR::Node *, TR::NodeChecklist &, TR::NodeChecklist &, NodeMap &);
   TR::Node *signExtend(TR::Node *, TR::NodeChecklist &, NodeMap &);
   TR::Node *signExtendBinOp(TR::ILOpCodes, TR::Node *, TR::NodeChecklist &, NodeMap &);
   void transmuteDescendantsIntoTruncations(TR::Node *, TR::Node *);
   void detectLoopsForIndVarConversion(TR_Structure *, TR::NodeChecklist &);
   void extendIVsOnLoopEntry(const TR::list<std::pair<int32_t, int32_t> > &, TR::Block *);
   void truncateIVsOnLoopExit(const TR::list<std::pair<int32_t, int32_t> > &, TR_RegionStructure *);
   void convertIV(TR::Node *, TR::TreeTop *, int32_t, int32_t, TR::ILOpCodes);
   void createConstraintsForNewInductionVariable(TR_Structure *, TR::SymbolReference *, TR::SymbolReference *);
   bool checkExpressionForInductionVariable(TR::Node *);
   bool checkStoreOfIndVar(TR::Node *);
   // end sign-extension

   TR::Node *placeInitializationTreeInLoopInvariantBlock(TR_BlockStructure *, TR::SymbolReference *,
         TR::SymbolReference *, int32_t, TR::SymbolReferenceTable *);
   TR::Node *placeNewInductionVariableIncrementTree(TR_BlockStructure *, TR::SymbolReference *, TR::SymbolReference *, int32_t, TR::SymbolReferenceTable *, TR::Node *, int32_t);
   TR::Node *placeNewInductionVariableIncrementTree(TR_BlockStructure *loopInvariantBlock, TR::SymbolReference *inductionVarSymRef, TR::SymbolReference *newSymbolReference, int32_t k, TR::SymbolReferenceTable *symRefTab, TR::Node *placeHolderNode, TR::Node *newLoad, TR::TreeTop *insertionTreeTop, TR::Node *constNode, bool isAddition);

   bool reassociateAndHoistComputations(TR::Block *, TR_Structure *);
   bool reassociateAndHoistComputations(TR::Block *, TR::Node *, int32_t, TR::Node *, vcount_t);

   bool reassociateAndHoistNonPacked();

   bool checkInvariance(TR::Node *, int32_t, TR::Node *, int32_t);

   int32_t maxInternalPointers()
         {
         // was int32_t reservedForLoops=comp()->getOption(TR_ProcessHugeMethods)?6:2;
         // increased to 8 since we did see the limit being reached and believe 2 would
         // be too little as a buffer for the internal pointer spills. 8 is a hypothetical
         // estimate and may be changed later if more observation is found.
         int32_t reservedForLoops=8;

         int32_t maxNumber=comp()->maxInternalPointers();

         if (maxNumber>=reservedForLoops)
            return maxNumber-reservedForLoops;
         else
            return reservedForLoops;
         }

   int32_t maxInternalPointersAtPointTooLateToBackOff()
         {
         int32_t reservedForLoops=comp()->getOption(TR_ProcessHugeMethods)?4:0;

         int32_t maxNumber=comp()->maxInternalPointers();

         if (maxNumber>=reservedForLoops)
            return maxNumber-reservedForLoops;
         else
            return reservedForLoops;
         }

   void    setAdditiveTermNode (TR::Node *node, int32_t k) { TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs); _linearEquations[k][3] = (intptrj_t) node; }
   TR::Node *getAdditiveTermNode(int32_t k) { TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs); return (TR::Node*)(intptrj_t)_linearEquations[k][3]; }
   TR::Node *duplicateAdditiveTermNode(int32_t k, TR::Node *node, TR::DataType type)
      {
      TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs);
      TR::Node *new_node = ((TR::Node*)(intptrj_t)_linearEquations[k][3])->duplicateTree();
      new_node->setByteCodeIndex(node->getByteCodeIndex());
      new_node->setInlinedSiteIndex(node->getInlinedSiteIndex());

      if (new_node->getDataType() != type)
          new_node = TR::Node::create(
                                     TR::ILOpCode::getProperConversion(new_node->getDataType(), type, false /* !wantZeroExtension */),
                                     1, new_node);
      return new_node;
      }

   bool isAdditiveTermConst(int32_t k)
        {
        TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs);
        return (((TR::Node*)(intptrj_t)_linearEquations[k][3]) == NULL ||
               ((TR::Node*)(intptrj_t)_linearEquations[k][3])->getOpCode().isLoadConst());
        }
   int64_t getAdditiveTermConst(int32_t k);
   bool isAdditiveTermEquivalentTo(int32_t k, TR::Node * node);

   void    setMulTermNode (TR::Node *node, int32_t k) { TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs); _linearEquations[k][2] = (intptrj_t) node; }
   TR::Node *getMulTermNode(int32_t k) { TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",k,_numberOfLinearExprs); return (TR::Node*)(intptrj_t)_linearEquations[k][2]; }
   TR::Node *duplicateMulTermNode(int32_t k, TR::Node *node, TR::DataType type);
   bool isMulTermConst(int32_t k)
      {
      TR_ASSERT(k < _numberOfLinearExprs, "index k %d exceeds _numberOfLinearExprs %d!\n",
                                          k, _numberOfLinearExprs);
      return ((TR::Node*)(intptrj_t)_linearEquations[k][2])->getOpCode().isLoadConst();
      }
   int64_t getMulTermConst(int32_t k);
   bool isMulTermEquivalentTo(int32_t k, TR::Node * node);
   bool isExprLoopInvariant(TR::Node *node)
      {
      if (node->getOpCode().isLoadConst() ||
          (node->getOpCode().isLoadVarDirect() &&
           node->getSymbol()->isAutoOrParm() &&
           _neverWritten->get(node->getSymbolReference()->getReferenceNumber())))
         return true;

      if (TR::ILOpCode::isOpCodeAnImplicitNoOp(node->getOpCode()) && node->getNumChildren() == 1)
         return isExprLoopInvariant(node->getChild(0));
      return false;
      }

   int64_t **_linearEquations;
   TR::Node **_loadUsedInNewLoopIncrement;
   TR::SymbolReference **_reassociatedAutos;
   List<TR::Node> _reassociatedNodes;

   List<TR_StoreTreeInfo> **_storeTreesList;
   //List<TR::Node> **_loadUsedInNewLoopIncrementList;

   int32_t _numSymRefs;
   SymRefPair **_hoistedAutos;
   SymRefPair *_parmAutoPairs;
   int32_t _count;
   int32_t _numberOfLinearExprs, _numInternalPointers;
   int32_t _numInternalPointerOrPinningArrayTempsInitialized;
   bool _usesLoadUsedInLoopIncrement;
   TR_StoreTreeInfo *_storeTreeInfoForLoopIncrement;
   bool _registersScarce;
   bool _newTempsCreated;
   bool _newNonAddressTempsCreated;

   // (64-bit)
   // sign-extension elimination data-structures
   bool _isInductionVariableMorphed;
   // end sign-extension

private:
   TR::Node* genLoad(TR::Node* node, TR::SymbolReference* symRef, bool isInternalPointer);
   void examineOpCodesForInductionVariableUse(TR::Node* node, TR::Node* parent, int32_t &childNum, int32_t &index, TR::Node* originalNode, TR::Node* replacingNode, TR::Node* linearTerm, TR::Node* mulTerm, TR::SymbolReference **newSymbolReference, TR::Block* loopInvariantBlock, TR::AutomaticSymbol* pinningArrayPointer, int64_t differenceInAdditiveConstants, bool &isInternalPointer, bool &downcastNode, bool &usingAladd);
   void changeBranchFromIntToLong(TR::Node* branch);
   TR::VPLongRange* genVPLongRange(TR::VPConstraint* cons, int64_t coeff, int64_t additive);
   TR::VPIntRange* genVPIntRange(TR::VPConstraint* cons, int64_t coeff, int64_t additive);
   };


enum TR_ProgressionKind {Identity = 0, Arithmetic, Geometric};

class TR_BasicInductionVariable
   {
   public:
   TR_ALLOC(TR_Memory::InductionVariableAnalysis);

   TR_BasicInductionVariable(TR::Compilation * c, TR_RegionStructure *loop, TR::SymbolReference *symRef)
      : _comp(c), _loop(loop), _symRef(symRef), _entryValue(0),
        _deltaOnBackEdge(0), _deltaOnExitEdge(0), _increment(0), _onlyIncrementValid(false)
      {}
   TR_BasicInductionVariable(TR::Compilation * c, TR_BasicInductionVariable *biv)
      : _comp(c), _loop(biv->_loop), _symRef(biv->_symRef), _entryValue(biv->_entryValue),
        _deltaOnBackEdge(biv->_deltaOnBackEdge), _deltaOnExitEdge(biv->_deltaOnExitEdge),
        _increment(biv->_increment)
      { _onlyIncrementValid = false; }

   TR::Compilation * comp() { return _comp; }

   TR::SymbolReference *getSymRef() { return _symRef; }

   TR::Node *getEntryValue() { return _entryValue; }

   bool isLongInt() { return _symRef->getSymbol()->getDataType() == TR::Int64; }

   void setEntryValue(TR::Node *node)
      { _entryValue = node->duplicateTree(); }

   int32_t getDeltaOnBackEdge() { return _deltaOnBackEdge; }
   void setDeltaOnBackEdge(int32_t d) { _deltaOnBackEdge = d; }

   int32_t getDeltaOnExitEdge() { return _deltaOnExitEdge; }
   void setDeltaOnExitEdge(int32_t d) { _deltaOnExitEdge = d; }

   int32_t getIncrement() { return _increment; }
   void setIncrement(int32_t incr) { _increment = incr; }

   virtual TR::Node *getExitValue() { return 0; }       // FIXME: ask the loop?

   // the following returns the iteration count of the loop,
   // note that this is not necessarily the same as the number of times
   // that this induction variable will be incremented
   //
   // iteration count is defined as the number of times the loop exit test
   // will fail (ie. the number of times the backedge will be taken)
   //
   virtual int32_t  getIterationCount() { return -1; } // FIXME: ask the loop?

   bool isOnlyIncrementValid() { return _onlyIncrementValid; }
   void setOnlyIncrementValid(bool v) { _onlyIncrementValid = v; }

   private:

   TR::Compilation *    _comp;
   TR_RegionStructure *_loop;
   TR::SymbolReference *_symRef;
   TR::Node            *_entryValue;
   int32_t             _deltaOnBackEdge;
   int32_t             _deltaOnExitEdge;
   int32_t             _increment;
   bool                _onlyIncrementValid;
   };

class TR_PrimaryInductionVariable : public TR_BasicInductionVariable
   {
   public:
   TR_ALLOC(TR_Memory::InductionVariableAnalysis);

   TR_PrimaryInductionVariable(TR_BasicInductionVariable *biv,
                               TR::Block *branchBlock,
                               TR::Node *exitBound, TR::ILOpCodes exitOp,
                               TR::Compilation *comp, TR::Optimizer *opt,
                               bool usesUnchangedValueInLoopTest, bool trace);

   TR::Block *getBranchBlock() { return _branchBlock; }
   TR::Node *getExitBound() { return _exitBound; }
   bool isUnsigned() { return ((TR::ILOpCode*)&_exitOp)->isUnsignedCompare(); }

   virtual TR::Node *getExitValue();
   virtual int32_t  getIterationCount() { return _iterCount; }

   int32_t getNumLoopExits() { return _numLoopExits; }
   void    setNumLoopExits(int32_t v) { _numLoopExits = v; }

   bool    usesUnchangedValueInLoopTest() { return _usesUnchangedValueInLoopTest; }
   void    setUsesUnchangedValueInLoopTest(bool v) { _usesUnchangedValueInLoopTest = v; }

   private:
   TR::Node            *_exitBound;
   TR::ILOpCodes        _exitOp;
   int32_t             _iterCount;
   TR::Block           *_branchBlock;
   int32_t             _numLoopExits;
   bool                _usesUnchangedValueInLoopTest;
   };

class TR_DerivedInductionVariable : public TR_BasicInductionVariable
   {
   public:
   TR_ALLOC(TR_Memory::InductionVariableAnalysis);
   TR_DerivedInductionVariable(TR::Compilation * c, TR_BasicInductionVariable *biv,
                               TR_PrimaryInductionVariable *piv)
      : TR_BasicInductionVariable(c, biv), _piv(piv) {}

   virtual TR::Node *getExitValue();
   virtual int32_t  getIterationCount() { return _piv->getIterationCount(); }

   private:
   TR_BasicInductionVariable *_piv;
   };

class TR_InductionVariableAnalysis : public TR::Optimization
   {
   public:
   TR_InductionVariableAnalysis(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_InductionVariableAnalysis(manager);
      }

   virtual int32_t perform();

   TR_Array<TR_BasicInductionVariable*> *getInductionVariables()
      {return _ivs; }

   private:

   class DeltaInfo
      {
      public:
      TR_ALLOC(TR_Memory::InductionVariableAnalysis);
      DeltaInfo(int32_t delta)
         : _delta(delta), _unknown(false), _kind(Identity) {}
      DeltaInfo(DeltaInfo *other)
         : _delta(other->_delta), _unknown(other->_unknown), _kind(other->_kind) {}

      void setUnknownValue() { _unknown = true; }
      bool isUnknownValue()  { return _unknown; }
      TR_ProgressionKind getKind() { return _kind; }
      int32_t getDelta() {return _delta; }

      void arithmeticDelta(int32_t incr);
      void geometricDelta (int32_t incr);
      void merge(DeltaInfo *other);

      private:
      int32_t            _delta;
      TR_ProgressionKind _kind;
      bool               _unknown;
      };

   struct AnalysisInfo
      {
      public:
      TR_ALLOC(TR_Memory::InductionVariableAnalysis);
      AnalysisInfo(TR_BitVector *loopLocalDefs, TR_BitVector *allDefs)
         : _loopLocalDefs(loopLocalDefs), _allDefs(allDefs) {}
      TR_BitVector *getLoopLocalDefs() { return _loopLocalDefs; }
      TR_BitVector *getAllDefs() { return _allDefs; }

      private:
      TR_BitVector *_loopLocalDefs;
      TR_BitVector *_allDefs;
      };


   void gatherCandidates(TR_Structure *s, TR_BitVector *b, TR_BitVector*);

   void perform(TR_RegionStructure *str);

   void analyzeNaturalLoop  (TR_RegionStructure *loop);
   void analyzeAcyclicRegion(TR_RegionStructure *region, TR_RegionStructure *loop);
   void analyzeCyclicRegion (TR_RegionStructure *region, TR_RegionStructure *loop);
   void analyzeBlock        (TR_BlockStructure  *block,  TR_RegionStructure *loop);
   void analyzeLoopExpressions(TR_RegionStructure *loop,
                               DeltaInfo **loopSet);

   bool analyzeExitEdges(TR_RegionStructure *loop, TR_BitVector *candidates,
                         TR_Array<TR_BasicInductionVariable*> &basicIVs);

   bool findEntryValues(TR_RegionStructure *loop,
                        TR_Array<TR_BasicInductionVariable*> &basicIVs);
   TR::Node *findEntryValueForSymRef(TR_RegionStructure *loop,
                                TR::SymbolReference *symRef);
   TR::Node *getEntryValue(TR::Block *block, TR::SymbolReference *symRef,
                          TR_BitVector *nodesDone,
                          TR_Array<TR::Node *> &cachedVales);

   bool isProgressionalStore(TR::Node *node, TR_ProgressionKind *kind, int64_t *incr);
   bool getProgression(TR::Node *expr, TR::SymbolReference *storeRef, TR::SymbolReference **ref, TR_ProgressionKind *prog, int64_t *incr);
   bool isGotoBlock(TR::Block *block);

   bool branchContainsInductionVariable(TR_RegionStructure *loop, TR::Node *branchNode, TR_Array<TR_BasicInductionVariable*> &basicIVs);
   bool branchContainsInductionVariable(TR::Node *branchNode, TR::SymbolReference *ivSymRef, int32_t *nodeBudget);
   bool isIVUnchangedInLoop(TR_RegionStructure *loop, TR::Block *loopTestBlock, TR::Node *ivNode);



   void initializeBlockInfoArray(TR_RegionStructure *region);
   DeltaInfo **setBlockInfo(TR::Block *b, DeltaInfo **info)
      { return _blockInfo[b->getNumber()] = info; }
   DeltaInfo **newBlockInfo(TR_RegionStructure *loop);
   DeltaInfo **getBlockInfo(TR::Block *block)
      { return _blockInfo[block->getNumber()]; }
   void        mergeWithBlock(TR::Block *block, DeltaInfo **info, TR_RegionStructure *loop);
   void        mergeWithSet(DeltaInfo **, DeltaInfo **, TR_RegionStructure *loop);

   void printDeltaInfo(DeltaInfo *info);

   DeltaInfo ***_blockInfo;
   TR_Array<TR_BasicInductionVariable*> *_ivs;
   TR_Dominators *_dominators;
   };

/*
 * This pass attempts to transforms loops with single back-edge testing address nodes into
 * ones with integer node tests. The main motivation is that IVA does not recognize address type node.
 * Since a lot of loop-based optimizations and analysis are written for loops with integral IV,
 * this is a simple solution to catch many address incrementing loops. The other, full-fledged solution is
 * to recognize address IV in IVA and everywhere else, but that may entail much more work.
 *
 * Address IV is also a fairly common coding pattern in C (pointer manip) and C++ with iterators
 * of contiguous memory.
 * This pass is analogous to loop strider in that it undo's the pointer incr which strides by size of underlying
 * type.
 */
class TR_IVTypeTransformer : public TR_LoopTransformer
   {
   public:
   TR_IVTypeTransformer(TR::OptimizationManager *manager)
      : TR_LoopTransformer(manager), _addrSymRef(NULL), _baseSymRef(NULL), _intIdxSymRef(NULL)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_IVTypeTransformer(manager);
      }

   virtual int32_t perform();

   private:
   void changeIVTypeFromAddrToInt(TR_RegionStructure *natLoop);
   bool isInAddrIncrementForm(TR::Node *node, int32_t &increment);
   void replaceAloadWithBaseIndexInSubtree(TR::Node *node);
   TR::ILOpCodes getIntegralIfOpCode(TR::ILOpCodes ifacmp, bool is64bit);
   TR::SymbolReference *findComparandSymRef(TR::Node *node);
   TR::SymbolReference *_addrSymRef;
   TR::SymbolReference *_baseSymRef;
   TR::SymbolReference *_intIdxSymRef;
   };


#endif
