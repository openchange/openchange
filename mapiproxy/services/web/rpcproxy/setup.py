#!/usr/bin/python

from distutils.core import setup

setup(name="rpcproxy",
      version="1.0",
      description="A RPC-over-HTTP implementation for Samba, using wsgi",
      author="Julien Kerihuel, Wolfgang Sourdeau",
      author_email="j.kerihuel@openchange.org, wsourdeau@inverse.ca",
      url="http://www.openchange.org/",
      scripts=["rpcproxy.wsgi"],
      packages=["rpcproxy"],
      requires=["openchange"]
)
