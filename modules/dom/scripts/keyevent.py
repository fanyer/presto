#
# Emit call to extend an object with a non-writable, non-configurable constant.
# Imported and called upon by the hardcore script generating what's required
# for key event handling.
#
import re

class DOM:
    @staticmethod
    def emitKeyConstant(op_key):
        """
        Generate the DOM code required to add a virtual key
        as a constant on an object. This allows scripts to
        refer to keycodes via symbolic names.

        @param op_key is the symbolic name of a Opera virtual
          key, and is assumed to be in the form OP_KEY_<suffix>
        """
        dom_vk_constant = re.sub("^OP_KEY_", "DOM_VK_", op_key)
        if dom_vk_constant:
            return 'PutNumericConstantL(object, "%s", %s, runtime);' % (dom_vk_constant, op_key)
        else:
            return ""
