/** @mainpage Jaypeg module
 *
 * This is an auto-generated documentation of the Jaypeg module.
 *
 * @section api API
 * 
 * To use this module you need to use create an image listener and a decoder.
 *
 * @subsection image_listener The image listener
 * JayImage is the main image listener which will receive all decoded image 
 * data and the size of the image. Simply implement this interface and pass it
 * to JayDecoder.
 * 
 * @subsection decoder The decoder
 * JayDecoder is the main decoder. It should be initilized with an image 
 * listener and then fed with data to decode the image.
 *
 * To decode a progressive jpeg you must also call JayDecoder::flushProgressive 
 * or no data will be written to the image listener. Only call 
 * JayDecoder::flushProgressive when you need the image data to be updated 
 * (after decoding is finished or when drawing the image).
 * 
 * @author Tim Johansson <timj@opera.com> 
*/
