
  precision highp float;

  const vec3 lightDir = vec3(0.577350269, 0.577350269, -0.577350269);
  varying vec3 vPosition;
  uniform vec3 cameraPos;
  uniform vec3 sphere1Center;
  uniform vec3 sphere2Center;
  uniform vec3 sphere3Center;

  bool intersectSphere(vec3 center, vec3 lStart, vec3 lDir,
                       out float dist) {
    vec3 c = center - lStart;
    float b = dot(lDir, c);
    float d = b*b - dot(c, c) + 1.0;
    if (d < 0.0) {
      dist = 10000.0;
      return false;
    }

    dist = b - sqrt(d);
    if (dist < 0.0) {
      dist = 10000.0;
      return false;
    }

    return true;
  }

  vec3 lightAt(vec3 N, vec3 V, vec3 color) {
    vec3 L = lightDir;
    vec3 R = reflect(-L, N);

    float c = 0.3 + 0.4 * pow(max(dot(R, V), 0.0), 30.0) + 0.7 * dot(L, N);

    if (c > 1.0) {
      return mix(color, vec3(1.6, 1.6, 1.6), c - 1.0);
    }

    return c * color;
  }

  bool intersectWorld(vec3 lStart, vec3 lDir, out vec3 pos,
                      out vec3 normal, out vec3 color) {
    float d1, d2, d3;
    bool h1, h2, h3;

    h1 = intersectSphere(sphere1Center, lStart, lDir, d1);
    h2 = intersectSphere(sphere2Center, lStart, lDir, d2);
    h3 = intersectSphere(sphere3Center, lStart, lDir, d3);

    if (h1 && d1 < d2 && d1 < d3) {
      pos = lStart + d1 * lDir;
      normal = pos - sphere1Center;
      color = vec3(0.0, 0.0, 0.9);
    }
    else if (h2 && d2 < d3) {
      pos = lStart + d2 * lDir;
      normal = pos - sphere2Center;
      color = vec3(0.9, 0.0, 0.0);
    }
    else if (h3) {
      pos = lStart + d3 * lDir;
      normal = pos - sphere3Center;
      color = vec3(0.0, 0.9, 0.0);
    }
    else if (lDir.y < -0.01) {
      pos = lStart + ((lStart.y + 2.7) / -lDir.y) * lDir;
      if (pos.x*pos.x + pos.z*pos.z > 30.0) {
        return false;
      }
      normal = vec3(0.0, 1.0, 0.0);
      if (fract(pos.x / 5.0) > 0.5 == fract(pos.z / 5.0) > 0.5) {
        color = vec3(1.0);
      }
      else {
        color = vec3(0.0);
      }
    }
    else {
     return false;
    }

    return true;
  }

  void main(void)
  {
    vec3 cameraDir = normalize(vPosition - cameraPos);

    vec3 p1, norm, p2;
    vec3 col, colT, colM, col3;
    if (intersectWorld(cameraPos, cameraDir, p1,
                       norm, colT)) {
      col = lightAt(norm, -cameraDir, colT);
      colM = (colT + vec3(0.7)) / 1.7;
      cameraDir = reflect(cameraDir, norm);
      if (intersectWorld(p1, cameraDir, p2, norm, colT)) {
        col += lightAt(norm, -cameraDir, colT) * colM;
        colM *= (colT + vec3(0.7)) / 1.7;
        cameraDir = reflect(cameraDir, norm);
        if (intersectWorld(p2, cameraDir, p1, norm, colT)) {
          col += lightAt(norm, -cameraDir, colT) * colM;
        }
      }

      gl_FragColor = vec4(col, 1.0);
    }
    else {
      gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
  }
