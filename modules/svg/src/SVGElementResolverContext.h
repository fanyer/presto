#ifndef SVG_ELEMENT_RESOLVER_CONTEXT
#define SVG_ELEMENT_RESOLVER_CONTEXT
# ifdef SVG_SUPPORT

class SVGDocumentContext;

class SVGElementResolver
{
private:
	OpPointerSet<HTML_Element> followed_elements;

public:
	SVGElementResolver() {}

	BOOL		IsLoop(HTML_Element* elm) const { return followed_elements.Contains(elm); }
	OP_STATUS	FollowReference(HTML_Element* elm)
	{
		OP_ASSERT(!followed_elements.Contains(elm));
		return followed_elements.Add(elm);
	}
	void		LeaveReference(HTML_Element* elm) { followed_elements.Remove(elm); }
};

class SVGElementResolverStack
{
private:
	SVGElementResolver* ctx;
	HTML_Element* last;

public:
	SVGElementResolverStack(SVGElementResolver* ctx) : ctx(ctx), last(NULL) {}
	~SVGElementResolverStack() { Pop(); }

	OP_STATUS Push(HTML_Element* elm)
	{
		RETURN_IF_ERROR(ctx->FollowReference(elm));

		last = elm;
		return OpStatus::OK;
	}
	void Pop()
	{
		if (last)
			ctx->LeaveReference(last);
	}
};

# endif // SVG_SUPPORT
#endif // SVG_ELEMENT_RESOLVER_CONTEXT
