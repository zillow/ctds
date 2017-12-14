#if defined(__GNUC__) && (__GNUC__ > 4)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#  pragma GCC diagnostic ignored "-Wlong-long"
#endif /* if defined(__GNUC__) && (__GNUC__ > 4) */

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4115)
#  pragma warning(disable: 4214)
#  pragma warning(disable: 4255)
#  pragma warning(disable: 4668)
#  pragma warning(disable: 4820)
#  pragma warning(disable: 4115)
#endif /* if defined(_MSC_VER) */
