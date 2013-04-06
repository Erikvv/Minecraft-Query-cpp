Description

- building blocks to query Minecraft servers
- using C++ and the boost::asio library

---

Goals

- simple interface, simple usage examples
- single-threaded base, but the possibility to query servers concurrently.
- imperative and object-oriented variants

---

Protocal support

1. the simple TCP protocol (used in the minecraft client when listing the servers) 
2. the basic stat of the [UDP/UT3/gamespot protocol](http://wiki.vg/Query)
3. the full stat of this protocol
4. the [ForgeEssentials snooper](http://github.com/ForgeEssentials/ForgeEssentialsMain/wiki/Snooper-Info)

The basic UDP stat is working, see example_main.cpp.
