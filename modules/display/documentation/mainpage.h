/** @mainpage The Display Module
The display module is responsible for various parts of the code that displays things (sic!).
The central classes in it are:
 - VisualDevice
 - StyleManager
 - CoreView

VisualDevice is the canvas that Opera draws on for displaying text, images and
lines on screen. VisualDevice also has a state comprising of mainly font, font
size, colour, line width and scroll position. Each document has at least one
VisualDevice, and each frame if the document has Netscape frames or iframes in
it. The VisualDevice is also used from the widgets code, each WidgetContainer
has a VisualDevice.

StyleManager keeps track of the different styles in Opera, eg for H1 -- H6, pre
etc. The styles are initialized from the logdoc module, in InitStylesL(). The
global StyleManager is created in DisplayModule::InitL(). StyleManager is also
involved in the fontswitching feature.

CoreView is Opera's internal implementation of a "view", ie a container that
can be clipped and take care of events (primarily mouse events). The reason for
having our own implementation is that most platforms can't have views that are
partly on top of each other, which gave us problems with iframes and plugins.

*/
