try:
    from select import poll, error, POLLIN, POLLOUT
    from errno import EINTR
except:
    from select import select, error
    from errno import EINTR

    POLLIN  = 1
    POLLOUT = 2

    class poll:
        def __init__(self):
            self.__fds = {}

        def register(self, fd, flags):
            self.__fds[fd] = flags

        def unregister(self, fd, flags):
            del self.__fds[fd]

        def poll(self, timeout=None):
            ready = []

            while True:
                rfds = [fd for fd, flags in self.__fds.items() if (flags & POLLIN) == POLLIN]
                wfds = [fd for fd, flags in self.__fds.items() if (flags & POLLOUT) == POLLOUT]

                rfds, wfds, efds = select(rfds, wfds, [], timeout)

                for fd in rfds: ready.append((fd, POLLIN))
                for fd in wfds: ready.append((fd, POLLOUT))

                return ready
