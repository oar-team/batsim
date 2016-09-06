#!/usr/bin/env python2
"""Simple HTTP server"""
from __future__ import unicode_literals
import atexit
import os
import sys
import argparse


class HTTPServerDaemon(object):

    """A HTTP server daemon class."""

    def __init__(self, root=os.getcwd()):
        """ Initialize the object."""
        self.root = root

    def daemonize(self, pidfile):
        """Deamonize class. UNIX double fork mechanism."""
        try:
            pid = os.fork()
            if pid > 0:
                # exit first parent
                sys.exit(0)
        except OSError as err:
            sys.stderr.write('fork #1 failed: {0}\n'.format(err))
            sys.exit(1)

        # decouple from parent environment
        os.chdir(self.root)
        os.setsid()
        os.umask(0)

        # do second fork
        try:
            pid = os.fork()
            if pid > 0:

                # exit from second parent
                sys.exit(0)
        except OSError as err:
            sys.stderr.write('fork #2 failed: {0}\n'.format(err))
            sys.exit(1)

        # redirect standard file descriptors
        sys.stdout.flush()
        sys.stderr.flush()
        si = open(os.devnull, 'r')
        so = open(os.devnull, 'a+')
        se = open(os.devnull, 'a+')

        os.dup2(si.fileno(), sys.stdin.fileno())
        os.dup2(so.fileno(), sys.stdout.fileno())
        os.dup2(se.fileno(), sys.stderr.fileno())

        # Make sure pid file is removed if we quit
        @atexit.register
        def delpid(self):
            os.remove(pidfile)

        # write pidfile
        pid = str(os.getpid())
        with open(pidfile, 'w+') as f:
            f.write(pid + '\n')

    def start(self, pidfile, *args, **kwargs):
        """Start the daemon."""
        # Check for a pidfile to see if the daemon already runs
        try:
            with open(pidfile, 'r') as pf:

                pid = int(pf.read().strip())
        except IOError:
            pid = None

        if pid:
            message = "pidfile {0} already exist. " + \
                      "Daemon already running?\n"
            sys.stderr.write(message.format(pidfile))
            sys.exit(1)

        # Start the daemon
        self.daemonize(pidfile)
        self.run(*args, **kwargs)

    def run(self, host, port):
        """ Run an HTTP server."""
        if sys.version_info[0] == 3:
            from http.server import HTTPServer, SimpleHTTPRequestHandler
            httpd = HTTPServer((host, port), SimpleHTTPRequestHandler)
        else:
            import SimpleHTTPServer
            import SocketServer
            handler = SimpleHTTPServer.SimpleHTTPRequestHandler
            httpd = SocketServer.TCPServer((host, port), handler)

        print("Running on http://%s:%s/" % (host, port))
        os.chdir(self.root)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            sys.stderr.write(u"\nBye\n")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description=sys.modules[__name__].__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('--port', action="store", default=9090, type=int,
                        help='Set the listening port')
    parser.add_argument('--root', action="store", default=os.getcwd())
    parser.add_argument('--bind', action="store", default="0.0.0.0",
                        help='Set the binding address')
    parser.add_argument('--daemon', action="store_true", default=False)
    parser.add_argument('--pid', action="store")

    try:
        args = parser.parse_args()
        http_server = HTTPServerDaemon(root=args.root)
        if args.daemon:
            if args.pid is None:
                parser.error("Need to set a pid file")
            http_server.start(args.pid, args.bind, args.port)
        else:
            http_server.run(args.bind, args.port)
    except Exception as exc:
        sys.stderr.write(u"\nError: %s\n" % exc)
        sys.exit(1)
