/*
 * Copyright 2019 The Open GEE Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ASSETTREE_H
#define ASSETTREE_H

#include <boost/graph/adjacency_list.hpp>
#include <functional>
#include <set>
#include <map>

#include "AssetVersion.h"
#include "autoingest/.idl/storage/AssetDefs.h"
#include "common/SharedString.h"
#include "StorageManager.h"

// This class efficiently updates the states of lots of asset versions at
// the same time. The idea is that you create a state updater, use it to
// perform one "macro" operation (such as a clean or resume), and then release
// it.
//
// Internally, this class represents the asset versions as a directed
// acyclic graph, and state updates are performed as graph operations.
class StateUpdater
{
  private:
    struct AssetVertex {
      SharedString name;
      AssetDefs::State state;
      bool inDepTree;
      bool recalcState;
      bool stateChanged;
      size_t index; // Used by the dfs function
    };

    enum DependencyType { INPUT, CHILD, DEPENDENT, DEPENDENT_AND_CHILD };

    struct AssetEdge {
      DependencyType type;
    };

    typedef boost::adjacency_list<
        boost::setS,
        boost::setS,
        boost::directedS,
        AssetVertex,
        AssetEdge> TreeType;
    typedef std::map<SharedString, TreeType::vertex_descriptor> VertexMap;

    // Helper struct for building the tree
    struct TreeBuildData;

    // Used by dfs function to update states of assets in the tree
    friend struct boost::property_map<TreeType, boost::vertex_index_t>;
    class SetStateVisitor;

    StorageManagerInterface<AssetVersionImpl> * const storageManager;
    TreeType tree;

    inline bool IsDependent(DependencyType type) { return type == DEPENDENT || type == DEPENDENT_AND_CHILD; }

    void BuildDependentTree(
        const SharedString & ref,
        std::function<bool(AssetDefs::State)> includePredicate);
    TreeType::vertex_descriptor AddOrUpdateVertex(
        const SharedString & ref,
        TreeBuildData & buildData,
        bool inDepTree,
        bool recalcState,
        std::set<TreeType::vertex_descriptor> & toFillIn);
    void FillInVertex(
        TreeType::vertex_descriptor myVertex,
        TreeBuildData & buildData,
        std::set<TreeType::vertex_descriptor> & toFillIn);
    void AddEdge(
        TreeType::vertex_descriptor from,
        TreeType::vertex_descriptor to,
        AssetEdge data);
    void SetState(
        TreeType::vertex_descriptor vertex,
        AssetDefs::State newState,
        bool sendNotifications);
  public:
    StateUpdater(StorageManagerInterface<AssetVersionImpl> * sm = &AssetVersion::storageManager()) : storageManager(sm) {}
    void SetStateForRefAndDependents(
        const SharedString & ref,
        AssetDefs::State newState,
        std::function<bool(AssetDefs::State)> updateStatePredicate);
};

#endif // ASSETTREE_H