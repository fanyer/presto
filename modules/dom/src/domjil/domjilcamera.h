/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILCAMERA_H
#define DOM_DOMJILCAMERA_H

#ifdef JIL_CAMERA_SUPPORT

#include "modules/hardcore/timer/optimer.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpCamera.h"
#include "modules/media/camerapreview.h"

class DOM_Element;

class DOM_JILCamera : public DOM_JILObject, public OpCameraCaptureListener
{
public:
	~DOM_JILCamera();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_CAMERA || DOM_Object::IsA(type); }
	virtual void GCTrace();

	static OP_STATUS Make(DOM_JILCamera*& new_jil_camera, DOM_Runtime* runtime);

	DOM_DECLARE_FUNCTION(captureImage);
	DOM_DECLARE_FUNCTION(startVideoCapture);
	DOM_DECLARE_FUNCTION(stopVideoCapture);
	DOM_DECLARE_FUNCTION(setWindow);
	enum { FUNCTIONS_ARRAY_SIZE = 5 };

protected:

	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	/** Camera capture success listener */
	virtual void OnCameraCaptured(OpCamera* camera, const uni_char* file_name);

	/** Camera capture failure listener */
	virtual void OnCameraCaptureFailed(OpCamera* camera, OP_CAMERA_STATUS error);
private:
	DOM_JILCamera();
	/** Handle OP_CAMERA_STATUS errors returned by camera functions.
	 *
	 * Note: don't use it for more generic errors because some OP_STATUS
	 * values are interpreted in a camera-specific way here.
	 */
	static int HandleCameraError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* runtime);

	virtual void OnCameraCaptureFinished(OP_STATUS result, const uni_char* file_name);

	CameraPreview* GetJILCameraPreview();
	OP_STATUS SetWindowInternal(DOM_Node *dom_node);

	ES_Object* m_on_camera_captured;
	DOM_Node* m_element;
};
#endif // JIL_CAMERA_SUPPORT

#endif // DOM_DOMJILCAMERA_H
