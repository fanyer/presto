/**

@mainpage Opera Template library

The <strong>Opera Template Library</strong> (OTL) aims to provide
<a href="http://en.wikipedia.org/wiki/Standard_Template_Library">STL</a>-like
containers in Core.

@section templates Templates

Unlike most existing container templates in Core, OTL templates
really gives you a container which stores type @c Foo if @c Foo is the
typename provided.

This if different from OpVector and OpHashTable classes, which would
give you a container which stores @c Foo* when the typename is @c Foo.

@code
OtlVector<Foo> foo;    // Stores: Foo
OpVector<Foo> foo2; // Stores: Foo*
@endcode

The library currently contains:

 - OtlVector - Dynamically growing array-like container.
 - OtlDeque - Double-ended queue, fast insertion in both ends.
 - OtlStack - A restricted type of OtlVector.
 - OtlList - Doubly-linked list.
 - OtlSet - Sorted set with unique elements.
 - OtlMap - Sorted, associative container.
 - OtlPair - Pair of two elements of arbitrary type.

More containers are planned, especially
<a href="http://en.wikipedia.org/wiki/Unordered_associative_containers_(C%2B%2B)">
unordered associative containers.</a>

@section examples Examples

@subsection vector OtlVector

@code
OtlVector<int> vector;

// Adding elements:
RETURN_IF_ERROR(vector.PushBack(13));
RETURN_IF_ERROR(vector.PushBack(37));
RETURN_IF_ERROR(vector.PushBack(42));

// Random access:
printf("Answer to everything: %i\n", vector[2]); // 42

// Iteration:
for (int i = 0; i < vector.GetSize(); ++i)
	printf("%i \n", vector[i]); // 13 37 42

// Removing elements:
vector.PopBack(); // Removes 42
vector.Clear(); // Removes all.
@endcode

@subsection deque OtlDeque

@code
struct Point
{
	Point(int x, int y) : x(x), y(y) {}
	int x;
	int y;
};

// ...

OtlDeque<Point> deque;

// Adding elements (Points are copied):
RETURN_IF_ERROR(deque.PushBack(Point(13, 37)));
RETURN_IF_ERROR(deque.PushBack(Point(18, 14)));
RETURN_IF_ERROR(deque.PushFront(Point(19, 84)));

// Random access:
printf("(%i, %i)\n", deque[0].x, deque[0].y); // (19, 84)
printf("(%i, %i)\n", deque[1].x, deque[1].y); // (13, 37)
printf("(%i, %i)\n", deque[2].x, deque[2].y); // (18, 14)

// Removing elements:
deque.Erase(1); // Removes (13, 37).
deque.Insert(1, Point(12, 34)); // Insert before item at index '1'.

printf("(%i, %i)\n", deque[0].x, deque[0].y); // (19, 84)
printf("(%i, %i)\n", deque[1].x, deque[1].y); // (12, 34)
printf("(%i, %i)\n", deque[2].x, deque[2].y); // (18, 14)
@endcode

@subsection map OtlMap

@code
OtlMap<int, float> map;

// Adding elements:
RETURN_IF_ERROR(map.Insert(13, 5.0));
RETURN_IF_ERROR(map.Insert(13, 6.0)); // Overwrite value @ 13.
RETURN_IF_ERROR(map.Insert(14, 7.5));

// Finding elements:
OtlMap<int, float>::Iterator foundIter = map.Find(13);
if(foundIter != map.End())
{
	// Found entry with key 13.
	const int key = foundIter.first;
	float value = foundIter.second;
	// Set different value:
	foundIter.second = 3.5;
}

// Iteration:
for (OtlMap<int, float>::Iterator i = map.Begin(); i != map.End(); ++i)
	printf("%f \n", i.second); // 3.5 7.5

// Removing elements:
map.Erase(13); // Removes element 13=>3.5
map.Clear(); // Removes all.
@endcode
*/
