import sys
import fileinput

from ctypes import *

try:
    from OpenGL.GLUT import *
    from OpenGL.GL import *
except:
    sys.exit("PyOpenGL not installed")

def CompileShader(source):
    program_object = glCreateProgram();
    shader_object = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(shader_object, "#version 110\n" + source);

    glCompileShader(shader_object);

    compile_status = c_int()
    glGetShaderiv(shader_object, GL_COMPILE_STATUS, compile_status);

    compile_log_length = c_int()
    glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, byref(compile_log_length));

    compile_log = create_string_buffer(compile_log_length.value)
    glGetShaderInfoLog(shader_object, compile_log_length, byref(compile_log_length), compile_log);

    if compile_log_length.value > 1:
        print compile_log.value    

    if compile_status.value == 0:
        return

    glAttachShader(program_object, shader_object);
    glDeleteShader(shader_object);

    glLinkProgram(program_object);

    link_status = c_int()
    glGetProgramiv(program_object, GL_LINK_STATUS, link_status);

    link_log_length = c_int()
    glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, byref(link_log_length));

    link_log = create_string_buffer(link_log_length.value)
    glGetProgramInfoLog(program_object, link_log_length, byref(link_log_length), link_log);

    if link_log_length.value > 1:
        print link_log.value

glutInit(sys.argv)
glutCreateWindow('sc')

if len(sys.argv) == 1:
    sys.exit("error: no arguments")

for fn in sys.argv[1:]:

    try:
        f = open(fn, "rb")
    except:
        sys.exit("error: no such file '" + fn + "'")

    shader_source = f.read()
    f.close()

    print fn + ":"

    CompileShader(shader_source)
