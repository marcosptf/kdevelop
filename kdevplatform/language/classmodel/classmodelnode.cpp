/*
 * KDevelop Class Browser
 *
 * Copyright 2007-2009 Hamish Rodda <rodda@kde.org>
 * Copyright 2009 Lior Mualem <lior.m.kde@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "classmodelnode.h"

#include <typeinfo>
#include <KLocalizedString>

#include "../duchain/duchainlock.h"
#include "../duchain/duchain.h"
#include "../duchain/persistentsymboltable.h"
#include "../duchain/duchainutils.h"
#include "../duchain/classdeclaration.h"
#include "../duchain/classfunctiondeclaration.h"
#include "../duchain/types/functiontype.h"
#include "../duchain/types/enumerationtype.h"

#include <debug.h>

using namespace KDevelop;
using namespace ClassModelNodes;

IdentifierNode::IdentifierNode(KDevelop::Declaration* a_decl,
                               NodesModelInterface* a_model,
                               const QString& a_displayName)
  : DynamicNode(a_displayName.isEmpty() ? a_decl->identifier().toString() : a_displayName, a_model)
  , m_identifier(a_decl->qualifiedIdentifier())
  , m_indexedDeclaration(a_decl)
  , m_cachedDeclaration(a_decl)
{
}

Declaration* IdentifierNode::declaration()
{
  if ( !m_cachedDeclaration )
    m_cachedDeclaration = m_indexedDeclaration.declaration();

  return m_cachedDeclaration.data();
}

bool IdentifierNode::getIcon(QIcon& a_resultIcon)
{
  DUChainReadLocker readLock(DUChain::lock());

  Declaration* decl = declaration();
  if ( decl )
    a_resultIcon = DUChainUtils::iconForDeclaration(decl);

  return !a_resultIcon.isNull();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

EnumNode::EnumNode(KDevelop::Declaration* a_decl, NodesModelInterface* a_model)
  : IdentifierNode(a_decl, a_model)
{
  // Set display name for anonymous enums
  if ( m_displayName.isEmpty() )
    m_displayName = QStringLiteral("*Anonymous*");
}

bool EnumNode::getIcon(QIcon& a_resultIcon)
{
  DUChainReadLocker readLock(DUChain::lock());

  ClassMemberDeclaration* decl = dynamic_cast<ClassMemberDeclaration*>(declaration());
  if ( decl == nullptr )
  {
    static QIcon Icon = QIcon::fromTheme(QStringLiteral("enum"));
    a_resultIcon = Icon;
  }
  else
  {
    if ( decl->accessPolicy() == Declaration::Protected )
    {
      static QIcon Icon = QIcon::fromTheme(QStringLiteral("protected_enum"));
      a_resultIcon = Icon;
    }
    else if ( decl->accessPolicy() == Declaration::Private )
    {
      static QIcon Icon = QIcon::fromTheme(QStringLiteral("private_enum"));
      a_resultIcon = Icon;
    }
    else
    {
      static QIcon Icon = QIcon::fromTheme(QStringLiteral("enum"));
      a_resultIcon = Icon;
    }
  }

  return true;
}

void EnumNode::populateNode()
{
  DUChainReadLocker readLock(DUChain::lock());

  Declaration* decl = declaration();

  if ( decl->internalContext() )
    foreach( Declaration* enumDecl, decl->internalContext()->localDeclarations() )
      addNode( new EnumNode(enumDecl, m_model) );
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

ClassNode::ClassNode(Declaration* a_decl, NodesModelInterface* a_model)
  : IdentifierNode(a_decl, a_model)
{
}

ClassNode::~ClassNode()
{
  if ( !m_cachedUrl.isEmpty() )
  {
    ClassModelNodesController::self().unregisterForChanges(m_cachedUrl, this);
    m_cachedUrl = IndexedString();
  }
}

void ClassNode::populateNode()
{
  DUChainReadLocker readLock(DUChain::lock());

  if ( m_model->features().testFlag(NodesModelInterface::ClassInternals) )
  {
    if ( updateClassDeclarations() )
    {
      m_cachedUrl = declaration()->url();
      ClassModelNodesController::self().registerForChanges(m_cachedUrl, this);
    }
  }

  // Add special folders
  if (m_model->features().testFlag(NodesModelInterface::BaseAndDerivedClasses))
    addBaseAndDerived();
}

template <> inline bool qMapLessThanKey(const IndexedIdentifier &key1, const IndexedIdentifier &key2)
{
  return key1.index() < key2.index();
}

bool ClassNode::updateClassDeclarations()
{
  bool hadChanges = false;
  SubIdentifiersMap existingIdentifiers = m_subIdentifiers;

  ClassDeclaration* klass = dynamic_cast<ClassDeclaration*>(declaration());

  if ( klass )
  {
    foreach(Declaration* decl, klass->internalContext()->localDeclarations())
    {
      // Ignore forward declarations.
      if ( decl->isForwardDeclaration() )
        continue;

      // Don't add existing declarations.
      if ( existingIdentifiers.contains( decl->ownIndex() ) )
      {
        existingIdentifiers.remove(decl->ownIndex());
        continue;
      }

      Node* newNode = nullptr;

      if ( EnumerationType::Ptr enumType = decl->type<EnumerationType>() )
        newNode = new EnumNode( decl, m_model );
      else if ( decl->isFunctionDeclaration() )
        newNode = new FunctionNode( decl, m_model );
      else if ( ClassDeclaration* classDecl = dynamic_cast<ClassDeclaration*>(decl) )
        newNode = new ClassNode(classDecl, m_model);
      else if ( ClassMemberDeclaration* memDecl = dynamic_cast<ClassMemberDeclaration*>(decl) )
        newNode = new ClassMemberNode( memDecl, m_model );
      else
      {
        // Debug - for reference.
        qCDebug(LANGUAGE) << "class: " << klass->toString() << "name: " << decl->toString() << " - unknown declaration type: " << typeid(*decl).name();
      }

      if ( newNode )
      {
        addNode(newNode);

        // Also remember the identifier.
        m_subIdentifiers.insert(decl->ownIndex(), newNode);

        hadChanges = true;
      }
    }
  }

  // Remove old existing identifiers
  for ( SubIdentifiersMap::iterator iter = existingIdentifiers.begin();
        iter != existingIdentifiers.end();
        ++iter )
  {
    iter.value()->removeSelf();
    m_subIdentifiers.remove(iter.key());
    hadChanges = true;
  }

  return hadChanges;
}

bool ClassNode::addBaseAndDerived()
{
  bool added = false;

  BaseClassesFolderNode *baseClassesNode = new BaseClassesFolderNode( m_model );
  addNode( baseClassesNode );
  if ( !baseClassesNode->hasChildren() )
    removeNode( baseClassesNode );
  else
    added = true;

  DerivedClassesFolderNode *derivedClassesNode = new DerivedClassesFolderNode( m_model );
  addNode( derivedClassesNode );
  if ( !derivedClassesNode->hasChildren() )
    removeNode( derivedClassesNode );
  else
    added = true;

  return added;
}

void ClassNode::nodeCleared()
{
  if ( !m_cachedUrl.isEmpty() )
  {
    ClassModelNodesController::self().unregisterForChanges(m_cachedUrl, this);
    m_cachedUrl = IndexedString();
  }

  m_subIdentifiers.clear();
}

void ClassModelNodes::ClassNode::documentChanged(const KDevelop::IndexedString&)
{
  DUChainReadLocker readLock(DUChain::lock());

  if ( updateClassDeclarations() )
    recursiveSort();
}

ClassNode* ClassNode::findSubClass(const KDevelop::IndexedQualifiedIdentifier& a_id)
{
  // Make sure we have sub nodes.
  performPopulateNode();

  /// @todo This is slow - we go over all the sub identifiers but the assumption is that
  ///       this function call is rare and the list is not that long.
  foreach(Node* item, m_subIdentifiers)
  {
    ClassNode* classNode = dynamic_cast<ClassNode*>(item);
    if ( classNode == nullptr )
      continue;

    if ( classNode->identifier() == a_id )
      return classNode;
  }

  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

FunctionNode::FunctionNode(Declaration* a_decl, NodesModelInterface* a_model)
  : IdentifierNode(a_decl, a_model)
{
  // Append the argument signature to the identifier's name (which is what the displayName is.
  if (FunctionType::Ptr type = a_decl->type<FunctionType>())
    m_displayName += type->partToString(FunctionType::SignatureArguments);

  // Add special values for ctor / dtor to sort first
  ClassFunctionDeclaration* classmember = dynamic_cast<ClassFunctionDeclaration*>(a_decl);
  if ( classmember ) {
    if ( classmember->isConstructor() || classmember->isDestructor() )
      m_sortableString = QLatin1Char('0') + m_displayName;
    else
      m_sortableString = QLatin1Char('1') + m_displayName;
  }
  else {
    m_sortableString = m_displayName;
  }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

ClassMemberNode::ClassMemberNode(KDevelop::ClassMemberDeclaration* a_decl, NodesModelInterface* a_model)
  : IdentifierNode(a_decl, a_model)
{
}

bool ClassMemberNode::getIcon(QIcon& a_resultIcon)
{
  DUChainReadLocker readLock(DUChain::lock());

  ClassMemberDeclaration* decl = dynamic_cast<ClassMemberDeclaration*>(declaration());
  if ( decl == nullptr )
    return false;

  if ( decl->isTypeAlias() )
  {
    static QIcon Icon = QIcon::fromTheme(QStringLiteral("typedef"));
    a_resultIcon = Icon;
  }
  else if ( decl->accessPolicy() == Declaration::Protected )
  {
    static QIcon Icon = QIcon::fromTheme(QStringLiteral("protected_field"));
    a_resultIcon = Icon;
  }
  else if ( decl->accessPolicy() == Declaration::Private )
  {
    static QIcon Icon = QIcon::fromTheme(QStringLiteral("private_field"));
    a_resultIcon = Icon;
  }
  else
  {
    static QIcon Icon = QIcon::fromTheme(QStringLiteral("field"));
    a_resultIcon = Icon;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

DynamicFolderNode::DynamicFolderNode(const QString& a_displayName, NodesModelInterface* a_model)
  : DynamicNode(a_displayName, a_model)
{
}

bool DynamicFolderNode::getIcon(QIcon& a_resultIcon)
{
  static QIcon folderIcon = QIcon::fromTheme(QStringLiteral("folder"));
  a_resultIcon = folderIcon;
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

FolderNode::FolderNode(const QString& a_displayName, NodesModelInterface* a_model)
  : Node(a_displayName, a_model)
{
}

bool FolderNode::getIcon(QIcon& a_resultIcon)
{
  static QIcon folderIcon = QIcon::fromTheme(QStringLiteral("folder"));
  a_resultIcon = folderIcon;
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

BaseClassesFolderNode::BaseClassesFolderNode(NodesModelInterface* a_model)
  : DynamicFolderNode(i18n("Base classes"), a_model)
{

}

void BaseClassesFolderNode::populateNode()
{
  DUChainReadLocker readLock(DUChain::lock());

  ClassDeclaration* klass = dynamic_cast<ClassDeclaration*>( static_cast<ClassNode*>(parent())->declaration() );
  if ( klass )
  {
    // I use the imports instead of the baseClasses in the ClassDeclaration because I need
    // to get to the base class identifier which is not directly accessible through the
    // baseClasses function.
    foreach( const DUContext::Import& import, klass->internalContext()->importedParentContexts() )
    {
      DUContext* baseContext = import.context( klass->topContext() );
      if ( baseContext && baseContext->type() == DUContext::Class )
      {
        Declaration* baseClassDeclaration = baseContext->owner();
        if ( baseClassDeclaration )
        {
          // Add the base class.
          addNode( new ClassNode(baseClassDeclaration, m_model) );
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

DerivedClassesFolderNode::DerivedClassesFolderNode(NodesModelInterface* a_model)
  : DynamicFolderNode(i18n("Derived classes"), a_model)
{

}

void DerivedClassesFolderNode::populateNode()
{
  DUChainReadLocker readLock(DUChain::lock());

  ClassDeclaration* klass = dynamic_cast<ClassDeclaration*>( static_cast<ClassNode*>(parent())->declaration() );
  if ( klass )
  {
    uint steps = 10000;
    const QList<Declaration*> inheriters = DUChainUtils::inheriters(klass, steps, true);

    for (Declaration* decl : inheriters) {
      addNode( new ClassNode(decl, m_model) );
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

Node::Node(const QString& a_displayName, NodesModelInterface* a_model)
  : m_parentNode(nullptr)
  , m_displayName(a_displayName)
  , m_model(a_model)
{
}

Node::~Node()
{
  // Notify the model about the removal of this nodes' children.
  if ( !m_children.empty() && m_model )
  {
    m_model->nodesAboutToBeRemoved(this, 0, m_children.size()-1);
    clear();
    m_model->nodesRemoved(this);
  }
}


void Node::clear()
{
  qDeleteAll(m_children);
  m_children.clear();
}

void Node::addNode(Node* a_child)
{
/// @note This is disabled for performance reasons - we add them to the bottom and a
///       sort usually follows which causes a layout change to be fired.
//   m_model->nodesAboutToBeAdded(this, m_children.size(), 1);
  a_child->m_parentNode = this;
  m_children.push_back(a_child);
//   m_model->nodesAdded(this);
}

void Node::removeNode(Node* a_child)
{
  int row = a_child->row();
  m_model->nodesAboutToBeRemoved(this, row, row);
  m_children.removeAt(row);
  delete a_child;
  m_model->nodesRemoved(this);
}

// Sort algorithm for the nodes.
struct SortNodesFunctor
{
  bool operator() (Node* a_lhs, Node* a_rhs)
  {
    if ( a_lhs->score() == a_rhs->score() )
    {
      return a_lhs->sortableString() < a_rhs->sortableString();
    }
    else
      return a_lhs->score() < a_rhs->score();
  }
};

void Node::recursiveSortInternal()
{
  // Sort my nodes.
  std::sort(m_children.begin(), m_children.end(), SortNodesFunctor());

  // Tell each node to sort it self.
  foreach (Node* node, m_children)
    node->recursiveSortInternal();
}

void Node::recursiveSort()
{
  m_model->nodesLayoutAboutToBeChanged(this);

  recursiveSortInternal();

  m_model->nodesLayoutChanged(this);
}

int Node::row()
{
  if ( m_parentNode == nullptr )
    return -1;

  return m_parentNode->m_children.indexOf(this);
}

QIcon ClassModelNodes::Node::cachedIcon()
{
  // Load the cached icon if it's null.
  if ( m_cachedIcon.isNull() )
  {
    if ( !getIcon(m_cachedIcon) )
      m_cachedIcon = QIcon();
  }

  return m_cachedIcon;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

DynamicNode::DynamicNode(const QString& a_displayName, NodesModelInterface* a_model)
  : Node(a_displayName, a_model)
  , m_populated(false)
{

}

void DynamicNode::collapse()
{
  performNodeCleanup();
}

void DynamicNode::expand()
{
  performPopulateNode();
}

void DynamicNode::performNodeCleanup()
{
  if ( !m_populated )
    return;

  if ( !m_children.empty() )
  {
    // Notify model for this node.
    m_model->nodesAboutToBeRemoved(this, 0, m_children.size()-1);

    // Clear sub-nodes.
    clear();

    m_model->nodesRemoved(this);
  }

  // This shouldn't be called from clear since clear is called also from the d-tor
  // and the function is virtual.
  nodeCleared();

  // Mark the fact that we've been collapsed
  m_populated = false;
}

void DynamicNode::performPopulateNode(bool a_forceRepopulate)
{
  if ( m_populated )
  {
    if ( a_forceRepopulate )
      performNodeCleanup();
    else
      return;
  }

  populateNode();

  // We're populated.
  m_populated = true;

  // Sort the list.
  recursiveSort();
}

bool DynamicNode::hasChildren() const
{
  // To get a true status, we'll need to populate the node.
  const_cast<DynamicNode*>(this)->performPopulateNode();

  return !m_children.empty();
}


// kate: space-indent on; indent-width 2; tab-width 4; replace-tabs on; auto-insert-doxygen on
