#ifndef __gst_marshal_MARSHAL_H__
#define __gst_marshal_MARSHAL_H__

#include	"platforms/media_backends/gst/include/glib-object.h"

G_BEGIN_DECLS

/* VOID:VOID (..\..\gst\gstmarshal.list:1) */
#define gst_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:BOOLEAN (..\..\gst\gstmarshal.list:2) */
#define gst_marshal_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN

/* VOID:INT (..\..\gst\gstmarshal.list:3) */
#define gst_marshal_VOID__INT	g_cclosure_marshal_VOID__INT

/* VOID:STRING (..\..\gst\gstmarshal.list:4) */
#define gst_marshal_VOID__STRING	g_cclosure_marshal_VOID__STRING

/* VOID:BOXED (..\..\gst\gstmarshal.list:5) */
#define gst_marshal_VOID__BOXED	g_cclosure_marshal_VOID__BOXED

/* VOID:BOXED,OBJECT (..\..\gst\gstmarshal.list:6) */
extern void gst_marshal_VOID__BOXED_OBJECT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:POINTER (..\..\gst\gstmarshal.list:7) */
#define gst_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:POINTER,OBJECT (..\..\gst\gstmarshal.list:8) */
extern void gst_marshal_VOID__POINTER_OBJECT (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:OBJECT (..\..\gst\gstmarshal.list:9) */
#define gst_marshal_VOID__OBJECT	g_cclosure_marshal_VOID__OBJECT

/* VOID:OBJECT,OBJECT (..\..\gst\gstmarshal.list:10) */
extern void gst_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:OBJECT,PARAM (..\..\gst\gstmarshal.list:11) */
extern void gst_marshal_VOID__OBJECT_PARAM (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:OBJECT,POINTER (..\..\gst\gstmarshal.list:12) */
extern void gst_marshal_VOID__OBJECT_POINTER (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:OBJECT,BOXED (..\..\gst\gstmarshal.list:13) */
extern void gst_marshal_VOID__OBJECT_BOXED (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:OBJECT,BOXED,STRING (..\..\gst\gstmarshal.list:14) */
extern void gst_marshal_VOID__OBJECT_BOXED_STRING (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:OBJECT,OBJECT,STRING (..\..\gst\gstmarshal.list:15) */
extern void gst_marshal_VOID__OBJECT_OBJECT_STRING (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* VOID:OBJECT,STRING (..\..\gst\gstmarshal.list:16) */
extern void gst_marshal_VOID__OBJECT_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:INT,INT (..\..\gst\gstmarshal.list:17) */
extern void gst_marshal_VOID__INT_INT (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* VOID:INT64 (..\..\gst\gstmarshal.list:18) */
extern void gst_marshal_VOID__INT64 (GClosure     *closure,
                                     GValue       *return_value,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data);

/* VOID:UINT,BOXED (..\..\gst\gstmarshal.list:19) */
extern void gst_marshal_VOID__UINT_BOXED (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* VOID:UINT,POINTER (..\..\gst\gstmarshal.list:20) */
#define gst_marshal_VOID__UINT_POINTER	g_cclosure_marshal_VOID__UINT_POINTER

/* BOOLEAN:VOID (..\..\gst\gstmarshal.list:21) */
extern void gst_marshal_BOOLEAN__VOID (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* BOOLEAN:POINTER (..\..\gst\gstmarshal.list:22) */
extern void gst_marshal_BOOLEAN__POINTER (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* POINTER:POINTER (..\..\gst\gstmarshal.list:23) */
extern void gst_marshal_POINTER__POINTER (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* BOXED:BOXED (..\..\gst\gstmarshal.list:24) */
extern void gst_marshal_BOXED__BOXED (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data);

G_END_DECLS

#endif /* __gst_marshal_MARSHAL_H__ */

