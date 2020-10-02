% OSPRay Studio Scene Graph

OSPRay Studio's scene graph (SG) is a tree structure that simplifies
construction and manipulation of OSPRay objects.  The SG is fundamentally a
tree data structure comprised of nodes. In contrast to the old OSPRay SG, the
new OSPRay Studio SG does _not_ have a 1:1 correspondence with OSPRay objects.
Instead, the OSPRay Studio SG presents a higher level interface that is easier
to understand and manipulate.

## SG Design Overview

The OSPRay Studio SG provides a high-level representation of OSPRay objects
that can be manipulated efficiently.  The SG is comprised of _nodes_, which can
represent OSPRay objects, custom data importers/exporters, and even individual
parameters for other nodes.  Nodes are connected with parent-child
relationships. Each node may have at most one parent, forming a tree structure.
This tree is easily traversed and searched, making manipulating the scene
heirarchy easy.

Nodes alone generally do not provide much functionality; instead, they manage
data and maintain the current state of the SG (there are some exceptions to
this detailed later).  Complex functionality, such as updating, committing, and
rendering is handled by _visitors_, which traverse the tree, performing various
actions based on nodes visited.

The general SG design is to build the scene heirarchy with nodes, then traverse
the SG with visitors.

---

## Nodes

The `Node` class, declared in `sg/Node.h`, is the backbone of the SG. `Node`
itself is an abstract base class. All node types inherit from it. This class
defines the basic API for working with nodes throughout the SG. A node is
primarily a data container, and does not provide specialized functionality.
This limited scope allows node types to be fairly short pieces of code.
Complex inter-node functionality is provided by the `Visitor` class, described
in the next section.

The code for the base `Node` class is organized as follows:

* Declarations for the base class, specialty classes, and convenience factory
  functions are in `sg/Node.h`
* Inline definitions for the base and specialty classes (template function
  definitions) are in `sg/Node.inl`
* All other definitions for the base class, specialty classes, factory
  functions, and node registrations are in `sg/Node.cpp`

### Creating Nodes

Four convenience factory functions are provided for creating nodes:

```cpp
NodePtr createNode(std::string name)
NodePtr createNode(std::string name, std::string subtype)
NodePtr createNode(std::string name, std::string subtype, Any value)
NodePtr createNode(std::string name,
                   std::string subtype,
                   std::string description,
                   Any val)
```

*Note: `NodePtr` is an alias for `std::shared_ptr<Node>`*

These are an easier alternative to constructing a node and setting values.
When creating a node, you must at least provide a name, but usually nodes are
created with a name, subtype, and value.

```cpp
NodePtr myNewNode = createNode("radius", "float", 1.f);
```

Note that these factory functions are not used for creating nodes that
represent OSPRay objects. These are described in [Special Node
Types](#special-node-types).

A node's `name` is how it is referenced throughout the SG. Multiple nodes in
the SG can have the same name, though you should avoid duplicate names between
sibling nodes!

A node's `subType` is a string used to find the correct symbol containing the
creator function for that type. For example, the node created above would need
the `float` creator function, which should be found in the
`ospray_create_sg_node__float` symbol. This special symbol name is created by
the `OSP_REGISTER_SG_NODE_NAME` macro, described more in detail in [Creating
Your Own Node Type](#creating-your-own-node-type).

A node's `value` is an `Any` type [^anytype].

### Node Properties

Nodes contain a private internal `properties` struct that holds the primary
data comprising the node. Most of the members of the `properties` struct have a
public getter and/or setter.

The `name`, `subType`, and `value` members of this struct come from the node's
creator (e.g. the factory functions described above). Some flags are provided
to give context to the node's `value`:

* `minMax` is used to set bounds on UI widgets created for this node (e.g.
  bound a light's brightness to [0, 1]). It does _not_ however prohibit setting
  values outside of this range in code
* `readOnly` is used to disable UI widget creation for this node. It does _not_
  however prohibit changing the node's value in code
* `sgOnly` is used to signal that this node is not a parameter that OSPRay will
  recognize during a commit. OSPRay Studio frequently provides abstract or
  derived values as parameters to the end user, and hides the potentially more
  complex OSPRay parameters internally. This flag lets us avoid sending the
  derived value to OSPRay

A node's `type` is a
`NodeType` value, which is an enum defined in `sg/NodeType.h`. The specific
type is set by the node's constructor. The base class constructor will set this
to `GENERIC`. The value of this is used in visitors to find nodes by the role
they play in the SG.

A node's connections to its parent and children are also held in `properties`.
Children are held in a `FlatMap`, with keys being child node names and values
being pointers to the child nodes [^flatmaptype].

Finally, a number of `TimeStamp` objects track the modification status of this
node and its children. These are used to determine if a subtree of the SG needs
to be processed and committed to OSPRay.

A node's `name` and `subType` can be accessed via its `name()` and `subType()`
methods, respectively. A node's `value` takes a bit more care, as the actual
value is wrapped in an `Any` object. The `Any` object itself can be accessed
with `value()`. To access the underlying value in its original type, you will
need to use `valueAs<T>()`, where you must know the type `T`.

The `value` can be set in two ways: using `setValue(T val)`, or using
`operator=(Any val)`. For example,

```cpp
NodePtr myNewNode = createNode("radius", "float");
myNewNode.setValue(1.f);
// OR
myNewNode = 1.f;
```

In either case, the node will mark itself as modified if the new value is
different from the current value. More detail on the modified status is
provided in [Tree Traversal Interface](#tree-traversal-interface).

### Parent-child Interface

Nodes are connected together with a parent-child relationship. A node should
have at most one parent, but can have any number of children. Nodes can be
created and added as a child in a few ways.  The most straightforward way is to
create the node(s), set the necessary values, and then use one of the `Node`
class' `add()` methods. For example,

```cpp
NodePtr myParentNode = createNode("mySpheres", "spheres");
NodePtr myChildNode = createNode("radius", "float", 1.f);
myParentNode->add(myChildNode);
```

Now, `myParentNode` has one child called `radius`. The `add()` method can take
a `Node &` or `NodePtr`.

Children may also be created and added to a parent node through factory
functions. These work similarly to the factory functions described above. There
are two factory functions provided:

```cpp
template <typename... Args>
Node &createChild(Args &&... args);

template <typename NODE_T, typename... Args>
NODE_T &createChildAs(Args &&... args);
```

The first function, `createChild()`, works identically to the factory functions
above. We can replicate our spheres/radius example like so:

```cpp
Node myChildNode = myParentNode->createChild("radius", "float", 1.f);
```

Note that the returned node is a base `Node` type.  The `createChildAs()`
variant can be used to create a child node and have the return type be the
actual derived node type. This is useful for creating an instance of a custom
node class that you will then need to manipulate (you avoid having to manually
cast the node). For example:

```cpp
auto &lightManager = myNode->createChildAs<sg::Lights>("lights", "lights");
// now call a derived-class method on our lightManager node
lightManager.addLight("ambientLight", "ambient");
```

Child nodes can be removed either with a reference or pointer to the node, or
by name.

```cpp
void remove(Node &node)
void remove(NodePtr node)
void remove(const std::string &name)
```

There are several ways of interacting with the children of a node. First, `bool
hasChildren()` can be used to check if a node has any children at all. A
specific child can be searched for using `bool hasChild(const std::string
&name)`. Note that this does not return the child node itself.

Child nodes can be accessed in 4 ways. The first two look for a child by name
and return a base `Node` object. These are useful for getting and setting child
node values.

```cpp
Node &child(const std::string &name)
Node &operator[](const std::string &name)

// get or set a child node's value
float radius = myParentNode.child("radius").valueAs<float>();
myParentNode["radius"] = 1.f;
```

There are also two methods that allow access to the derived node type. They are
indentical except that the first returns a reference, and the second a pointer.

```cpp
template <typename NODE_T>
NODE_T &childAs(const std::string &name)

template <typename NODE_T>
std::shared_ptr<NODE_T> childNodeAs(const std::string &name)
```

Similar to the factory functions, these are useful if you need to access
derived class methods of the node.

### Tree Traversal Interface

Once nodes are connected into a tree, the tree needs to be traversed in order
to translate it into an OSPRay render graph.  A node's `traverse` method
provides an "entry point" for tree traversal.

Tree traversal is performed mainly by `Visitor` objects, which are described in
more detail in [Visitors](#visitors). A node's `commit()` and `render()`
methods are actually shortcuts to calling `traverse()` using the appropriate
visitor.

A node can check its modified status using
`bool isModified()`. This will return `true` whenever any of a node's children
are marked as modified (i.e. a node is considered "modified" if and only if one
or more of its children are modified)

### UI Generation

UI is generated automatically, though `Node`s can affect the generated widgets.
UI generation will be discussed either in a GUI section, or in the UI generator
`Visitor`'s section.

### Special Considerations

Any other structural notes, C++-specific design principles (such as copy/move deletion), and any other unwritten `Node` lore.

`Node`s cannot be copied or moved; this includes forbidding the use of copy
constructors as well as `operator=` for other `Node`s. This design avoids some
edge cases where SG structure may become fragile. Suppose we have `Node a = b`,
where `b` is also a `Node`. Now, `b`'s parent contains two identical `Node`s --
including the same name. When accessing that name, which node should be
affected? Or, should `a` replace `b` entirely? We avoid these cases altogether.

### Special Node Types

There are two main special `Node` types: `Node_T` and `OSPNode`.

### OSPRay Studio SG Structure

The actual structure of the SG in concrete terms: root `Frame` node, its
`OSPNode` children, and so on.

### Creating Your Own Node Type

The basic steps in creating your own node type. This is where experiential
information comes into play. Things like, create children in the constructor,
setting UI limiters and SG-only flags, registering it, settings its type, etc.

[^anytype]: The `Any` type comes from `rkcommon`, in `utility/Any.h`. It
  provides a C++11 interface similar to C++17's `std::any`.

[^flatmaptype]: The `FlatMap` type comes from `rkcommon`, in
  `utility/FlatMap.h`. It provides a similar interface to `std::map`, but uses
  an `std::vector` for faster lookups.

---

## Visitors

The `Visitor` class defines objects used to traverse the SG and perform complex
functions based on the nodes encountered. A `Visitor` can be called from the
root node to traverse the whole tree, or be limited to a subtree.

`Visitor`s are essentially functors -- objects treated like functions. As such,
the functionality provided by a `Visitor` is defined in its `operator()`
method. The `operator()` method is called each time the `Visitor` object visits
a new `Node` during traversal. In this method, `Visitor`s can perform any
function based on the current `Node`. For example, the `Commit` visitor will
commit the `Node` if it is marked as modified.

The `bool` return value determines whether children of the current `Node`
should be visited. That is, it provides an early-exit for traversal to increase
performance. For example, the `Commit` visitor will return `false` if the
current `Node` was not marked as modified, since there is no reason to check
the children.

When a `Node` is "revisited" during traversal (i.e. after a `Node`'s children
have been traversed), the `postChildren()` method is called.

### RenderScene Visitor

This visitor is responsible for creating instances of geometries and volumes that are added to a World. It builds OSPRay scene heirarchy for rendering and is called everytime we want to render a new scene graph. Traversal of scene graph with this visitor allows for geometries to be matched with their surface representation(material etc.) and encapsulated in a `Geometric model`, volumes to be matched with their rendering apperance and encapsulated in a `Volumetric model`. It then collects geometric and volumetric models that belong to a single `OSPRay Group` and create an `Instance` to be placed in the World from this group.

### Commit Visitor

This visitor is responsible for executing `preCommit()` and `postCommit()` methods of every `Node` in the subtree on which it is called based on their last modified and commit history. These methods interact with OSPRay API(`ospSetParam`) for setting parameter on the corresponding OSPRay object for the node and executing commit for allowing the parameters to take effect on the object.


### GenerateImGuiWidgets Visitor

This visitor is responsible for creating `imgui widgets` for every corresponding node of the subtree it is traversing.

---

## Material Registry

This class implements a material list for all imported materials with a model or a scene and manages insertion or deletion of new materal nodes for a scene. 