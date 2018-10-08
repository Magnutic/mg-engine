layout(location = 0) in vec3 vert_position;
layout(location = 8) in uint _matrix_index;

struct matrices_t {
    mat4 MVP;
    mat4 M;
};

layout(std140) uniform MatrixBlock {
    matrices_t matrices[MATRIX_ARRAY_SIZE];
} _matrix_block;

#define MATRIX_MVP (_matrix_block.matrices[_matrix_index].MVP)
#define MATRIX_M (_matrix_block.matrices[_matrix_index].M)

void main()
{
    // Transform vertex location to projection space
    gl_Position = MATRIX_MVP * vec4(vert_position, 1.0);
}

