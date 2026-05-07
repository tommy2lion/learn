# Refactoring Plan

I've reviewed our implementation code and believe it is time for a refactoring.

## (1) Coding Conventions
- (1-1) Although we're using C, I still want to adopt an object-oriented approach. `struct` is essentially an interface and also a class.  
  We can use:
  ```c
  #define struct interface
  #define struct class
  ```
- (1-2) I personally prefer lowercase naming. For example, consider a memory manager object. My preferred style is:
  ```c
  #define SMALL_BUFFER_SIZE
  class tagt_mem_manager 
  {
      uchar buffer[SMALL_BUFFER_SIZE];
  } mem_manager;
  ```
  Function names can follow a style like `mem_manager_init_block`, and variable names should adopt a similar pattern.
- (1-3) In summary, I'm not very fond of Hungarian notation with uppercase letters in function or variable names. For instance, instead of `GDP`, I'd rather write `gdp`.
- (1-4) The only place I'm willing to use uppercase is in macros.

## (2) Raylib Usage
I prefer an interface-oriented approach when using raylib, meaning there should be an encapsulation layer (interface layer) in between.  
Our application code should only call this encapsulation layer, so that I can easily swap out raylib for something like nanoVG or Cairo.  
Therefore, we need to introduce an interface class (`igraph`) and an implementation class (`graph_raylib`).

## (3) Platform Encapsulation
I also want our code to have platform encapsulation. That is, wherever Windows APIs are used, wrap them with an interface layer.  
These APIs should preferably have functionally similar counterparts on Linux.  
Thus, an interface class (`iplatform`) and an implementation class (`platform_windows`) are required.

## (4) UI Abstraction
Graphical interface elements such as layouts, buttons, canvases (circuit canvas and timing diagram canvas), input boxes, and menus should be abstracted into objects. A `frame` class should manage them.  
This `frame` object handles drawing management, including multiple layers (draw background first, then top layer), partial refresh, window sizing, etc. This part should be abstracted into a framework.  
For example, the circuit canvas and timing diagram canvas are just instantiations of a canvas object, each with its own `draw` function.

## (5) Focus Management
The framework should have a concept of focus, with a focused object. This way, refreshing can be limited to only what is necessary (no need to redraw the entire screen).

## (6) Graphics Buffer
If needed, introduce a graphics buffer class between the `frame` and the low-level graphics library.

## (7) Input Message Dispatch
Keyboard and mouse messages should have a proper dispatch mechanism, sending them to the currently focused object. We can introduce a `quit_manager` object to intercept events like the ESC key.

## (8) Reusable Framework
The above essentially describes a graphical UI framework. It should be a cohesive, reusable framework.

## (9) Digital Circuit Application
Our digital circuit application is an application built on top of this framework.

## (10) Extensibility of Application Layer
The design of the digital circuit application layer objects must fully consider extensibility. For example, our `component` class may need to support a `chipset` class in the future.

## (11) Saving Circuit State
Saving the digital circuit should be designed around pixel positions. That is, wherever a point is originally drawn, it should appear at the same location when reopened.

## (12) Separation of Circuit and Components
Therefore, we may need to separate the circuit from the components.

## (13) Object Factory
All object creation must use an object factory, which can be split into a framework factory and an application factory.

## (14) Design Patterns
Necessary design patterns should be used. Depend on interfaces, not concrete objects. Object responsibility assignments must be well-planned.

If we complete the above refactoring, or rather rewrite—the old code was mainly for understanding the requirements.

These are my initial thoughts for now. Please first produce a design proposal. It can actually be divided into two parts:  
One part is the **framework design**, perhaps a lightweight framework similar to MFC. Of course, MFC's GUI is quite ugly, so we'll use our own graphics library.  
The other part is the **application design**, where you should also describe the object design involved in the digital circuit simulation (DCS).
