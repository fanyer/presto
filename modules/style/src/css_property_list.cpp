/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/tempbuf.h"
#include "modules/style/css_property_list.h"
#include "modules/style/css.h"

CSS_property_list::~CSS_property_list()
{
    while (decl_list.First())
    {
        CSS_decl* temp = static_cast<CSS_decl*>(decl_list.First());

        temp->Out(); // Updates first
        temp->Unref();
    }
}

void CSS_property_list::AddDeclL(int property, const uni_char* value, int value_len, BOOL important, CSS_decl::DeclarationOrigin origin, CSS_string_decl::StringType type, BOOL source_local)
{
	CSS_string_decl* new_cssd = OP_NEW_L(CSS_string_decl, (property, type, source_local));

	if (new_cssd->CopyAndSetString(value, value_len) == OpStatus::ERR_NO_MEMORY)
	{
		OP_DELETE(new_cssd);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, uni_char* value, BOOL important, CSS_decl::DeclarationOrigin origin, CSS_string_decl::StringType type, BOOL source_local)
{
	ANCHOR_ARRAY(uni_char, value);

	CSS_string_decl* new_cssd = OP_NEW_L(CSS_string_decl, (property, value, type, source_local));

	ANCHOR_ARRAY_RELEASE(value);

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddLongDeclL(int property, long value, BOOL important, CSS_decl::DeclarationOrigin origin)
{
    CSS_long_decl* new_cssd = OP_NEW_L(CSS_long_decl, (property, value));

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddColorDeclL(int property, COLORREF value, BOOL important, CSS_decl::DeclarationOrigin origin)
{
    CSS_long_decl* new_cssd = OP_NEW_L(CSS_color_decl, (property, (long)value));

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddTypeDeclL(int property, CSSValue value, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_type_decl* new_cssd = OP_NEW_L(CSS_type_decl, (property, value));

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, short* val_array, int val_array_len, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_array_decl* new_cssd = OP_NEW_L(CSS_array_decl, (property));

	OP_STATUS err = new_cssd->Construct(val_array, val_array_len);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(new_cssd);
		LEAVE(err);
	}

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, CSS_generic_value* val_array, int val_array_len, int layer_count, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_gen_array_decl* new_cssd;
	LEAVE_IF_NULL(new_cssd = CSS_copy_gen_array_decl::Create(property, val_array, val_array_len));

	new_cssd->SetLayerCount(layer_count);
	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, const CSS_generic_value_list& val_list, int layer_count, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_gen_array_decl* new_cssd;
	LEAVE_IF_NULL(new_cssd = CSS_copy_gen_array_decl::Create(property, val_list));

	if (!new_cssd)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	new_cssd->SetLayerCount(layer_count);
	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, float val, int val_type, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_number_decl* new_cssd = OP_NEW_L(CSS_number_decl, (property, val, val_type));

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, float val1, float val2, int val1_type, int val2_type, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_number2_decl* new_cssd = OP_NEW_L(CSS_number2_decl, (property, val1, val2, val1_type, val2_type));

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDeclL(int property, short types[4], float values[4], BOOL important, CSS_decl::DeclarationOrigin origin)
{
	CSS_number4_decl* new_cssd = OP_NEW_L(CSS_number4_decl, (property));

	for (int i=0; i<4; i++)
		new_cssd->SetValue(i, types[i], values[i]);

	new_cssd->Ref();
	new_cssd->Into(&decl_list);
	if (important)
		new_cssd->SetImportant();
	new_cssd->SetOrigin(origin);
}

void CSS_property_list::AddDecl(CSS_decl* decl, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	if (important)
		decl->SetImportant();
	decl->SetOrigin(origin);
	decl->Ref();
	decl->Into(&decl_list);
}

void CSS_property_list::ReplaceDecl(CSS_decl* decl, BOOL important, CSS_decl::DeclarationOrigin origin)
{
	// Find last occurrence of the property.
	CSS_decl* target = GetLastDecl();
	while (target && target->GetProperty() != decl->GetProperty())
		target = target->Pred();

	decl->Ref();

	if (important)
		decl->SetImportant();
	decl->SetOrigin(origin);

	// Insert at the end if no occurrence is found. Otherwise, replace
	// the target in-place.
	if (!target)
		decl->Into(&decl_list);
	else
	{
		decl->Follow(target);
		target->Out();
		target->Unref();
	}
}

// Leave if out of memory
CSS_property_list* CSS_property_list::GetCopyL()
{
	CSS_property_list* pl = OP_NEW(CSS_property_list, ());
	if (pl == NULL)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	OpStackAutoPtr<CSS_property_list> proplist_ptr(pl);

	CSS_decl* new_css_decl;
	CSS_decl* css_decl = GetFirstDecl();
	while (css_decl)
	{
		new_css_decl = 0;
		short prop = css_decl->GetProperty();
		switch (css_decl->GetDeclType())
		{
		case CSS_DECL_TYPE:
			new_css_decl = OP_NEW_L(CSS_type_decl, (prop, css_decl->TypeValue()));
			break;
		case CSS_DECL_LONG:
			new_css_decl = OP_NEW_L(CSS_long_decl, (prop, css_decl->LongValue()));
			break;
		case CSS_DECL_COLOR:
			new_css_decl = OP_NEW_L(CSS_color_decl, (prop, css_decl->LongValue()));
			break;
		case CSS_DECL_ARRAY:
			new_css_decl = CSS_array_decl::Create(prop, css_decl->ArrayValue(), css_decl->ArrayValueLen());
			if (!new_css_decl)
				LEAVE(OpStatus::ERR_NO_MEMORY);
			break;
		case CSS_DECL_GEN_ARRAY:
			new_css_decl = CSS_copy_gen_array_decl::Copy(prop, static_cast<CSS_gen_array_decl *>(css_decl));
			if (!new_css_decl)
				LEAVE(OpStatus::ERR_NO_MEMORY);
			break;
		case CSS_DECL_STRING:
			{
				const uni_char* str = css_decl->StringValue();
				if (str)
				{
					new_css_decl = OP_NEW_L(CSS_string_decl, (prop, ((CSS_string_decl*)css_decl)->GetStringType(), ((CSS_string_decl*)css_decl)->IsSourceLocal()));
					if (((CSS_string_decl*)new_css_decl)->CopyAndSetString(str, uni_strlen(str)) == OpStatus::ERR_NO_MEMORY)
						LEAVE(OpStatus::ERR_NO_MEMORY);
				}
			}
			break;
		case CSS_DECL_NUMBER:
			new_css_decl = OP_NEW_L(CSS_number_decl, (prop, css_decl->GetNumberValue(0), css_decl->GetValueType(0)));
			break;
		case CSS_DECL_NUMBER2:
			new_css_decl = OP_NEW_L(CSS_number2_decl, (prop, css_decl->GetNumberValue(0), css_decl->GetNumberValue(1), css_decl->GetValueType(0), css_decl->GetValueType(1)));
			break;
		case CSS_DECL_NUMBER4:
			{
				CSS_number4_decl* new_num4_decl = OP_NEW_L(CSS_number4_decl, (prop));
				new_css_decl = new_num4_decl;
				for (int i=0; i<4; i++)
					new_num4_decl->SetValue(i, css_decl->GetValueType(i), css_decl->GetNumberValue(i));
			}
			break;
		case CSS_DECL_TRANSFORM_LIST:
			{
				new_css_decl = css_decl->CreateCopy();
				if (!new_css_decl)
					LEAVE(OpStatus::ERR_NO_MEMORY);
			}
			break;
		default:
			OP_ASSERT(FALSE); // we shouldn't be here.
			break;
		}

		if (new_css_decl)
		{
			if (css_decl->GetImportant())
				new_css_decl->SetImportant();
			new_css_decl->SetOrigin(css_decl->GetOrigin());

			new_css_decl->Ref();
			new_css_decl->Into(&(pl->decl_list));
		}

		css_decl = css_decl->Suc();
	}

	return proplist_ptr.release();
}

CSS_decl* CSS_property_list::RemoveDecl(int property)
{
	CSS_decl* removed_decl = NULL;
	CSS_decl* old_decl = (CSS_decl*)decl_list.First();
	while (old_decl)
	{
		if (old_decl->GetProperty() == property)
		{
			if (removed_decl)
			{
				removed_decl->Out();
				removed_decl->Unref();
			}
			removed_decl = old_decl;
		}
		old_decl = old_decl->Suc();
	}

	if (removed_decl)
		removed_decl->Out();
	return removed_decl;
}

/*static*/ unsigned int
CSS_property_list::GetPropertyFlags(CSS_decl* decl)
{
	unsigned int flags = HAS_NONE;
	switch (decl->GetProperty())
	{
	case CSS_PROPERTY_content:
		flags |= HAS_CONTENT;
	case CSS_PROPERTY__o_border_image:
	case CSS_PROPERTY_background_image:
	case CSS_PROPERTY_list_style_image:
		flags |= HAS_URL_PROPERTY;
		break;
#ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition_duration:
	case CSS_PROPERTY_transition_delay:
		flags |= HAS_TRANSITIONS;
		break;
# ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_name:
		flags |= HAS_ANIMATIONS;
		break;
# endif // CSS_ANIMATIONS
#endif // CSS_TRANSITIONS
	}
	return flags;
}

unsigned int
CSS_property_list::PostProcess(BOOL delete_duplicates, BOOL get_flags)
{
	// If none of the params are TRUE, this call is a waste.
	OP_ASSERT(delete_duplicates || get_flags);

	unsigned int prop_flags(HAS_NONE);

	CSS_decl* last_decl = GetLastDecl();
	while (last_decl)
	{
		CSS_decl* rm_decl = NULL;

		if (delete_duplicates)
		{
			CSS_decl* curr_decl = last_decl->Pred();
			int property = last_decl->GetProperty();
			BOOL prio = last_decl->GetImportant();

			while (curr_decl)
			{
				if (curr_decl->GetProperty() == property)
				{
					if (rm_decl)
					{
						rm_decl->Out();
						rm_decl->Unref();
					}

					if (!prio && curr_decl->GetImportant())
					{
						rm_decl = last_decl;
						break;
					}
					else
						rm_decl = curr_decl;
				}
				curr_decl = curr_decl->Pred();
			}
		}

		if (get_flags && last_decl != rm_decl)
			prop_flags |= GetPropertyFlags(last_decl);

		last_decl = last_decl->Pred();

		if (rm_decl)
		{
			if (rm_decl == last_decl)
				last_decl = last_decl->Pred();
			rm_decl->Out();
			rm_decl->Unref();
		}
	}

	return prop_flags;
}

BOOL CSS_property_list::AddList(CSS_property_list* list, BOOL force_important)
{
	BOOL has_changed = FALSE;
	CSS_decl* new_decl = list->GetFirstDecl();
	while (new_decl)
	{
		if (force_important)
			new_decl->SetImportant();

		CSS_decl* old_decl = (CSS_decl*)decl_list.Last();
		CSS_decl* replace_decl = NULL;
		while (old_decl)
		{
			if (old_decl->GetProperty() == new_decl->GetProperty())
			{
				if (!replace_decl || old_decl->GetImportant() && !replace_decl->GetImportant())
				{
					if (replace_decl)
					{
						replace_decl->Out();
						replace_decl->Unref();
					}
					replace_decl = old_decl;
				}
				else
				{
					CSS_decl* tmp_decl = old_decl;
					old_decl = old_decl->Pred();
					tmp_decl->Out();
					tmp_decl->Unref();
					continue;
				}
			}
			old_decl = old_decl->Pred();
		}

		if (replace_decl &&
			(replace_decl->GetImportant() != new_decl->GetImportant() ||
			 !replace_decl->IsEqual(new_decl)))
		{
			replace_decl->Out();
			replace_decl->Unref();
			replace_decl = NULL;
		}

		CSS_decl* in_decl = new_decl;
		new_decl = new_decl->Suc();

		if (!replace_decl)
		{
			in_decl->Out();
			in_decl->Into(&decl_list);
			has_changed = TRUE;
		}
	}
	return has_changed;
}

void CSS_property_list::AppendPropertiesAsStringL(TempBuffer* tmp_buf)
{
	CSS_decl *first = GetFirstDecl();
	for (CSS_decl* decl = first; decl; decl = decl->Suc())
	{
		if (decl != first)
			tmp_buf->AppendL(" ");
		CSS::FormatCssPropertyL(decl->GetProperty(), tmp_buf);
		CSS::FormatDeclarationL(tmp_buf, decl, FALSE, CSS_FORMAT_CSSOM_STYLE);
		if (decl->GetImportant())
			tmp_buf->AppendL(" !important");

		tmp_buf->AppendL(";");
	}
}

#ifdef STYLE_PROPNAMEANDVALUE_API

OP_STATUS CSS_property_list::GetPropertyNameAndValue(unsigned int idx, TempBuffer* name, TempBuffer* value, BOOL& important)
{
	if (idx < GetLength())
	{
		CSS_decl* OP_MEMORY_VAR decl = GetFirstDecl();
		while (idx-- > 0)
			decl = decl->Suc();
		decl = CSS_MatchRuleListener::ResolveDeclaration(decl);

		char prop_name[CSS_PROPERTY_NAME_MAX_SIZE+1]; /* ARRAY OK 2008-01-25 rune */
		int prop = decl->GetProperty();
		if (prop == CSS_PROPERTY_font_color)
			prop = CSS_PROPERTY_color;
		op_strncpy(prop_name, GetCSS_PropertyName(prop), CSS_PROPERTY_NAME_MAX_SIZE);
		prop_name[CSS_PROPERTY_NAME_MAX_SIZE] = '\0';
		op_strlwr(prop_name);
		RETURN_IF_ERROR(name->Append(prop_name));
		TRAP_AND_RETURN(ret, CSS::FormatDeclarationL(value, decl, FALSE, CSS_FORMAT_COMPUTED_STYLE));
		important = decl->GetImportant();
	}
	return OpStatus::OK;
}

#endif // STYLE_PROPNAMEANDVALUE_API
