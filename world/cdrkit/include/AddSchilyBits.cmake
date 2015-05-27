IF(NOT CHECKED_rols)
   SET(CHECKED_rols 1)

   LIST(APPEND EXTRA_LIBS "rols")

# abuse this include file to make sure the target is set

   IF(NOT MANSUBDIR)
      SET(MANSUBDIR "share/man")
   ENDIF(NOT MANSUBDIR)

ENDIF(NOT CHECKED_rols)

