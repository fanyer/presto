/** @mainpage libvega module
 *
 * This is an auto-generated documentation of the libvega module.
 *
 * @section api API
 * 
 * libvega is the vector graphics library used in Opera. It is used for both
 * SVG and canvas.
 *
 * Using the module requires building a path and sending it to the renderer.
 * You can also set gradient or pattern fills and apply filters to the rendered
 * images.
 *
 * VEGARenderer is the main class which handles creation of most other objects, 
 * with the exception of VEGAPath and VEGATransform.
 *
 * To render a path, create a VEGARenderer and use it to create a render target.
 * Set the render target as active and set a color, then call the fillPath 
 * method of VEGARenderer to draw the path. You have to create and build the 
 * VEGAPath object which you want to draw and pass it to fillPath.
 *
 * @author Tim Johansson <timj@opera.com> 
*/
