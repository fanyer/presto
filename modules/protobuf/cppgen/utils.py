import os

cachePath = os.path.join("modules", "protobuf", "cache")

# Original topological sort code written by Ofer Faigon (www.bitformation.com) and used with permission
class LoopError(Exception):
    pass

def inserted_sorted(coll, item, key=None):
    if not key:
        key = lambda i: i
    item_key = key(item)
    for idx, cur in enumerate(coll):
        if item_key < key(cur):
            coll.insert(idx, item)
            return
    coll.insert(len(coll), item)

def stable_topological_sort(items, partial_order):
    """Perform topological sort.
       items is a list or iterator of items to be sorted.
       partial_order is a list or iterator of pairs. If pair (a,b) is in it, it means
       that item a should appear before item b.
       Returns a list of the items in one of the possible orders, or raises
       LoopError if partial_order contains a loop.
       This is a stable sort so the relative order of the elements are kept.

    >>> list(stable_topological_sort(['a', 'b', 'c', 'd'], [('a', 'c')]))
    ['a', 'b', 'c', 'd']
    >>> list(stable_topological_sort(['a', 'b', 'c', 'd'], [('a', 'c'), ('b', 'c')]))
    ['a', 'b', 'c', 'd']
    >>> list(stable_topological_sort(['a', 'b', 'c', 'd'], [('a', 'c'), ('b', 'c'), ('d', 'c')]))
    ['a', 'b', 'd', 'c']
    >>> list(stable_topological_sort(['a', 'b', 'c', 'd'], [('a', 'c'), ('b', 'c'), ('d', 'c'), ('d', 'a')]))
    ['b', 'd', 'a', 'c']
    >>> list(stable_topological_sort(['b', 'a', 'c', 'd'], [('a', 'c'), ('b', 'c')]))
    ['b', 'a', 'c', 'd']
    >>> list(stable_topological_sort(['a', 'b', 'c', 'd'], [('a', 'c'), ('b', 'c'), ('b', 'a')]))
    ['b', 'a', 'c', 'd']
    >>> list(stable_topological_sort(['a', 'b', 'c', 'd'], [('a', 'c'), ('b', 'c'), ('c', 'd'), ('d', 'a')]))
    Traceback (most recent call last):
        ...
    LoopError: Loop in DAG, cannot sort
    """
    graph = {}
    for idx, item in enumerate(items):
        if item not in graph:
            graph[item] = [0, idx]
    for a,b in partial_order:
        graph[a].append(b)
        graph[b][0] += 1
    roots = [(node, info[1]) for (node, info) in graph.items() if info[0] == 0]
    roots.sort(key=lambda i: i[1])
    while roots:
        root, root_idx = roots.pop(0)
        yield root
        for child in graph[root][2:]:
            graph[child][0] -= 1
            if graph[child][0] == 0:
                child_i = graph[child][1]
                inserted_sorted(roots, (child, child_i), key=lambda i: i[1])
        del graph[root]
    if graph:
        raise LoopError("Loop in DAG, cannot sort")

def makeRelativeFunc(opera_root, relative_path=None):
    """
    Creates a function which makes relative paths.
    The path supplied to the new function are first made absolute (relative
    to opera_root). Then they are made relative to relative_path.
    Returns the new function.

    :param str opera_root: The root path to use for incoming relative paths.
    :param str relative_path: The path which all new paths are made relative to. If not supplied uses opera_root as path.
    """
    opera_root = os.path.normpath(os.path.abspath(opera_root))
    if relative_path:
        relative_path = os.path.normpath(os.path.abspath(relative_path))
    else:
        relative_path = opera_root
    def makeRelative(fname):
        """Returns the path of fname relative to the opera_root"""
        return os.path.relpath(fname, relative_path).replace(os.path.sep, '/')
    return makeRelative
