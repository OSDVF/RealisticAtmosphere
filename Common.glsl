//?#version 440

/*Maps from [0,1] to [-0.5, 1.5]*/
float GetTextureCoordFromUnitRange(float x, float texture_size) {
  return 0.5 / texture_size + x * (1.0 - 1.0 / texture_size);
}

float GetUnitRangeFromTextureCoord(float u, float texture_size) {
  return (u - 0.5 / texture_size) / (1.0 - 1.0 / texture_size);
}