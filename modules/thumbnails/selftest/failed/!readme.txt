Note!

Testcases that render text may fail since the font rendering mechanism depends on the build type.
Desktop uses platform freetype or win32 platform equivalents while core uses bundled freetype on both win32 and linux.
The reference images in this module were generated with the core build, so they will fail with desktop builds.

In case you are making changes to the thumbnails module, it is advised to run the selftests on a clean module first.
In case you get any failures on testcases that include text rendering and the only difference between the failed and
reference images is the font shape/size, you might want to use the failed images as the reference ones later on in
order to verify the selftest run result after introducing your changes.
