//
//  HierarchyTreeNode.h
//  UIEditor
//
//  Created by Yuri Coder on 10/15/12.
//
//

#ifndef __UIEditor__HierarchyTreeNode__
#define __UIEditor__HierarchyTreeNode__

#include <set>
#include "DAVAEngine.h"
#include <QString>
#include "HierarchyTreeNodeExtraData.h"

using namespace DAVA;

// Base class for all Hierarchy Tree Nodes.
class HierarchyTreeNode
{
public:
    // Type definitions for the Tree Node.
	typedef std::list<HierarchyTreeNode*> HIERARCHYTREENODESLIST;
    typedef HIERARCHYTREENODESLIST::iterator HIERARCHYTREENODESITER;
    typedef HIERARCHYTREENODESLIST::const_iterator HIERARCHYTREENODESCONSTITER;
    
	typedef int HIERARCHYTREENODEID;
	typedef std::set<HIERARCHYTREENODEID> HIERARCHYTREENODESIDLIST;

  	static const HIERARCHYTREENODEID HIERARCHYTREENODEID_EMPTY = -1;

    HierarchyTreeNode(const QString& name);
	HierarchyTreeNode(const HierarchyTreeNode* node);
    virtual ~HierarchyTreeNode();
    
    // Add the node to the list.
    void AddTreeNode(HierarchyTreeNode* treeNode);
    
    // Remove the node from the list, return TRUE if succeeded.
    bool RemoveTreeNode(HierarchyTreeNode* treeNode, bool needDelete = true);
    
    // Access to the nodes list.
    const HIERARCHYTREENODESLIST& GetChildNodes() const;
    
	void SetName(const QString name) {this->name = name;};
    const QString& GetName() const {return name;};
	
	HIERARCHYTREENODEID GetId() const {return id;};
	
    // Access to the node extra data.
    HierarchyTreeNodeExtraData& GetExtraData() {return extraData;};
	
	virtual void SetParent(HierarchyTreeNode* /*node*/){};
	bool IsHasChild(const HierarchyTreeNode* node) const;

protected:
	HIERARCHYTREENODEID id;
	
	QString name;
	
    // Cleanup the list of tree nodes.
    void Cleanup();
    
    // List of child nodes.
    HIERARCHYTREENODESLIST childNodes;
    
    // Tree node extra data.
    HierarchyTreeNodeExtraData extraData;
	
	static HIERARCHYTREENODEID nextId;
};


#endif /* defined(__UIEditor__HierarchyTreeNode__) */
