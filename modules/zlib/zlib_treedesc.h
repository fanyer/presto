#ifndef MODULES_ZLIB_TREEDESC_H
#define MODULES_ZLIB_TREEDESC_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*****************************************************************************/
/*              static_tree_desc_s - originally in trees.h                   */
/*****************************************************************************/

struct static_tree_desc_s {
    const struct ct_data_s *static_tree;  /* static tree or NULL */
    const int *extra_bits;      /* extra bits for each code or NULL */
    int     extra_base;          /* base index for extra_bits */
    int     elems;               /* max number of elements in the tree */
    int     max_length;          /* max bit length for the codes */
};

struct static_tree_desc_s* get_l_desc();
struct static_tree_desc_s* get_d_desc();
struct static_tree_desc_s* get_bl_desc();

void InitZLibTreeDescs(struct static_tree_desc_s* _static_l_desc,
                       struct static_tree_desc_s* _static_d_desc,
                       struct static_tree_desc_s* _static_bl_desc);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MODULES_ZLIB_TREEDESC_H
