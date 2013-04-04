/* For use by autoheader. */

#ifndef CONFIG_H
#define CONFIG_H    /* To stop multiple inclusions. */


/* Enable GNU extensions. */
#undef _GNU_SOURCE

/* How many arguments does gethostbyname_r() take? */
#undef HAVE_FUNC_GETHOSTBYNAME_R_6
#undef HAVE_FUNC_GETHOSTBYNAME_R_5
#undef HAVE_FUNC_GETHOSTBYNAME_R_3

/* Define to 'int' if not already defined by the system. */
#undef socklen_t


@TOP@
/* autoheader generated things inserted here. */
@BOTTOM@


#endif /* CONFIG_H */

