/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"    // precompiled header...
#include "nsMsgHeaderParser.h"
#include "nsISimpleEnumerator.h"
#include "comi18n.h"
#include "prmem.h"
#include "nsMemory.h"
#include <ctype.h>
#include "nsAlgorithm.h"
#include "nsMsgUtils.h"
#include "nsStringGlue.h"
#include <algorithm>
#include "js/Value.h"
#include "mozilla/mailnews/MimeHeaderParser.h"
#include "mozilla/mailnews/Services.h"

nsresult FillResultsArray(const char * aName, const char *aAddress, char16_t ** aOutgoingEmailAddress, char16_t ** aOutgoingName,
                          char16_t ** aOutgoingFullName);

/*
 * Macros used throughout the RFC-822 parsing code.
 */

#undef FREEIF
#define FREEIF(obj) do { if (obj) { PR_Free (obj); obj = 0; }} while (0)

#define COPY_CHAR(_D,_S)            do { if (!_S || !*_S) { *_D++ = 0; }\
                                         else { int _LEN = NextChar_UTF8((char *)_S) - _S;\
                                                memcpy(_D,_S,_LEN); _D += _LEN; } } while (0)
//#define NEXT_CHAR(_STR)             (_STR = (* (char *) _STR < 128) ? (char *) _STR + 1 : NextChar_UTF8((char *)_STR))
#define NEXT_CHAR(_STR)             (_STR = NextChar_UTF8((char *)_STR))
#define TRIM_WHITESPACE(_S,_E,_T)   do { while (_E > _S && IS_SPACE(_E[-1])) _E--;\
                                         *_E++ = _T; } while (0)

// This is what MIME considers to be whitespace.
#define kWhitespace " \t\r\n"

/*
 * The following are prototypes for the old "C" functions used to support all of the RFC-822 parsing code
 * We could have made these private functions of nsMsgHeaderParser if we wanted...
 */
static int msg_parse_Header_addresses(const char *line, char **names, char **addresses,
                                      bool quote_names_p = true, bool quote_addrs_p = true,
                                      bool first_only_p = false);
static int msg_quote_phrase_or_addr(char *address, int32_t length, bool addr_p);
static nsresult msg_unquote_phrase_or_addr(const char *line, bool strict, char **lineout);
#if 0
static char *msg_format_Header_addresses(const char *addrs, int count,
                                         bool wrap_lines_p);
#endif

static char *msg_remove_duplicate_addresses(const nsACString &addrs,
                                            const nsACString &other_addrs);
static char *msg_make_full_address(const char* name, const char* addr);

/*
 * nsMsgHeaderParser definitions....
 */

nsMsgHeaderParser::nsMsgHeaderParser()
{
}

nsMsgHeaderParser::~nsMsgHeaderParser()
{
}

NS_IMPL_ISUPPORTS1(nsMsgHeaderParser, nsIMsgHeaderParser)

// helper function called by ParseHeadersWithArray
nsresult FillResultsArray(const char * aName, const char *aAddress, char16_t ** aOutgoingEmailAddress, char16_t ** aOutgoingName,
                          char16_t ** aOutgoingFullName)
{
  *aOutgoingFullName = nullptr;
  *aOutgoingEmailAddress = nullptr;
  *aOutgoingName = nullptr;

  nsAutoCString result;
  if (aAddress && aAddress[0])
  {
    MIME_DecodeMimeHeader(aAddress, NULL, false, true, result);
    *aOutgoingEmailAddress = ToNewUnicode(NS_ConvertUTF8toUTF16(!result.IsEmpty() ? result.get() : aAddress));
  }

  if (aName && aName[0])
  {
    MIME_DecodeMimeHeader(aName, NULL, false, true, result);
    *aOutgoingName = ToNewUnicode(NS_ConvertUTF8toUTF16(!result.IsEmpty() ? result.get() : aName));
  }

  nsCString fullAddress;
  nsCString unquotedAddress;
  fullAddress.Adopt(msg_make_full_address(aName, aAddress));
  if (!fullAddress.IsEmpty())
  {
    MIME_DecodeMimeHeader(fullAddress.get(), nullptr, false, true, result);
    if (!result.IsEmpty())
      fullAddress = result;
    nsMsgHeaderParser::UnquotePhraseOrAddr(fullAddress.get(), true, getter_Copies(unquotedAddress));
    if (!unquotedAddress.IsEmpty())
      fullAddress = unquotedAddress;
    *aOutgoingFullName = ToNewUnicode(NS_ConvertUTF8toUTF16(fullAddress));
  }
  else
    *aOutgoingFullName = nullptr;

  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::ParseHeadersWithArray(const char16_t * aLine, char16_t *** aEmailAddresses,
                                                       char16_t *** aNames, char16_t *** aFullNames, uint32_t * aNumAddresses)
{
  char * names = nullptr;
  char * addresses = nullptr;
  uint32_t numAddresses = 0;
  nsresult rv = NS_OK;

  // need to convert unicode to UTF-8...
  nsAutoString tempString (aLine);
  char * utf8String = ToNewUTF8String(tempString);

  rv = ParseHeaderAddresses(utf8String, &names, &addresses, &numAddresses);
  NS_Free(utf8String);
  if (NS_SUCCEEDED(rv) && numAddresses)
  {
    // allocate space for our arrays....
    *aEmailAddresses = (char16_t **) PR_MALLOC(sizeof(char16_t *) * numAddresses);
    *aNames = (char16_t **) PR_MALLOC(sizeof(char16_t *) * numAddresses);
    *aFullNames = (char16_t **) PR_MALLOC(sizeof(char16_t *) * numAddresses);

    // for simplicities sake...
    char16_t ** outgoingEmailAddresses = *aEmailAddresses;
    char16_t ** outgoingNames = *aNames;
    char16_t ** outgoingFullNames = *aFullNames;

    // iterate over the results and fill in our arrays....
    uint32_t index = 0;
    const char * currentName = names;
    const char * currentAddress = addresses;
    char * unquotedName = nullptr;
    while (index < numAddresses)
    {
      if (NS_SUCCEEDED(UnquotePhraseOrAddr(currentName, true, &unquotedName)))
        rv = FillResultsArray(unquotedName, currentAddress, &outgoingEmailAddresses[index], &outgoingNames[index], &outgoingFullNames[index]);
      else
        rv = FillResultsArray(currentName, currentAddress, &outgoingEmailAddresses[index], &outgoingNames[index], &outgoingFullNames[index]);

      PR_FREEIF(unquotedName);
      currentName += strlen(currentName) + 1;
      currentAddress += strlen(currentAddress) + 1;
      index++;
    }
  }

  *aNumAddresses = numAddresses;
  PR_FREEIF(names);
  PR_FREEIF(addresses);
  return rv;
}

nsresult
nsMsgHeaderParser::ParseHeaderAddresses(const char *aLine, char **aNames,
                                        char **aAddresses,
                                        uint32_t *aNumAddresses)
{
  NS_ENSURE_ARG_POINTER(aNumAddresses);

  int status = msg_parse_Header_addresses(aLine, aNames, aAddresses);
  if (status < 0)
    return NS_ERROR_FAILURE;

  *aNumAddresses = static_cast<uint32_t>(status);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgHeaderParser::RemoveDuplicateAddresses(const nsACString &aAddrs,
                                            const nsACString &aOtherAddrs,
                                            nsACString &aResult)
{
  nsCString res;
  res.Adopt(msg_remove_duplicate_addresses(aAddrs, aOtherAddrs));
  aResult.Assign(res);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgHeaderParser::MakeMimeAddress(const nsAString &aName,
                                   const nsAString &aAddress, nsAString &aResult)
{
  nsCString utf8Str;
  utf8Str.Adopt(msg_make_full_address(NS_ConvertUTF16toUTF8(aName).get(),
                                      NS_ConvertUTF16toUTF8(aAddress).get()));
  CopyUTF8toUTF16(utf8Str, aResult);
  return NS_OK;
}

nsresult nsMsgHeaderParser::UnquotePhraseOrAddr (const char *line, bool preserveIntegrity, char** result)
{
  NS_ENSURE_ARG_POINTER(result);
  return msg_unquote_phrase_or_addr(line, preserveIntegrity, result);
}

nsresult nsMsgHeaderParser::UnquotePhraseOrAddrWString (const char16_t *line, bool preserveIntegrity, char16_t ** result)
{
  nsCString utf8Str;
  nsresult rv = msg_unquote_phrase_or_addr(NS_ConvertUTF16toUTF8(line).get(), preserveIntegrity, getter_Copies(utf8Str));
  if (NS_SUCCEEDED(rv))
  {
    *result = ToNewUnicode(NS_ConvertUTF8toUTF16(utf8Str.get()));
    if (*result == nullptr)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

 /* this function will be used by the factory to generate an RFC-822 Parser....*/
nsresult NS_NewHeaderParser(nsIMsgHeaderParser ** aInstancePtrResult)
{
  /* note this new macro for assertions...they can take a string describing the assertion */
  NS_PRECONDITION(nullptr != aInstancePtrResult, "nullptr ptr");
  if (nullptr != aInstancePtrResult)
  {
    nsMsgHeaderParser* parser = new nsMsgHeaderParser();
    if (parser)
      return parser->QueryInterface(NS_GET_IID(nsIMsgHeaderParser), (void **)aInstancePtrResult);
    else
      return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
  }
  else
    return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}


/*
 * The remainder of this file is the actual parsing code and it was extracted verbatim from addrutils.cpp
 */

/* msg_parse_Header_addresses
 *
 * Given a string which contains a list of Header addresses, parses it into
 * their component names and mailboxes.
 *
 * The returned value is the number of addresses, or a negative error code;
 * the names and addresses are returned into the provided pointers as
 * consecutive null-terminated strings.  It is up to the caller to free them.
 * Note that some of the strings may be zero-length.
 *
 * Either of the provided pointers may be NULL if the caller is not interested
 * in those components.
 *
 * quote_names_p and quote_addrs_p control whether the returned strings should
 * be quoted as Header entities, or returned in a more human-presentable (but
 * not necessarily parsable) form.
 *
 * If first_only_p is true, then only the first element of the list is
 * returned; we don't bother parsing the rest.
 */
static int msg_parse_Header_addresses (const char *line, char **names, char **addresses,
                bool quote_names_p, bool quote_addrs_p, bool first_only_p)
{
  uint32_t addr_count = 0;
  size_t line_length;
  const char *line_end;
  const char *this_start;
  char *name_buf = 0, *name_out, *name_start;
  char *addr_buf = 0, *addr_out, *addr_start;

  if (names)
    *names = 0;
  if (addresses)
    *addresses = 0;
  NS_ASSERTION(line, "");
  if (!line)
    return -1;
  line_length = strlen(line);
  if (line_length == 0)
    return 0;

  name_buf = (char *)PR_Malloc(line_length * 2 + 10);
  if (!name_buf)
    return -1;

  addr_buf = (char *)PR_Malloc(line_length * 2 + 10);
  if (!addr_buf)
  {
    FREEIF(name_buf);
    return -1;
  }

  line_end = line;
  addr_out = addr_buf;
  name_out = name_buf;
  name_start = name_buf;
  addr_start = addr_buf;
  this_start = line;

  /* Skip over extra whitespace, commas or semicolons before addresses. */
  while (*line_end && (IS_SPACE(*line_end) || *line_end == ',' ||
                       *line_end == ';'))
    NEXT_CHAR(line_end);

  while (*line_end)
  {
    uint32_t paren_depth = 0;
    const char *oparen = 0;
    const char *mailbox_start = 0;
    const char *mailbox_end = 0;
    bool in_group = false;

    while (*line_end &&
           // comma is ok inside () and <>
           !(*line_end == ',' && paren_depth <= 0 &&
             (!mailbox_start || mailbox_end)) &&
           // semi-colon is also ok inside the same and also as long as we're not in the middle of a group.
           !(*line_end == ';' && !in_group && paren_depth <= 0 &&
             (!mailbox_start || mailbox_end)))
    {
      if (*line_end == '\\')
      {
        line_end++;
        if (!*line_end) /* otherwise, we walk off end of line, right? */
          break;
      }
      else if (*line_end == '\"')
      {
        int leave_quotes = 0;

        line_end++;  /* remove open " */

        /* handle '"John.Van Doe"@space.com' case */
        if (paren_depth == 0 && !mailbox_start)
        {
          const char *end_quote;
          /* search for the closing quote but ignored escaped quote \" */
          for (end_quote = line_end;; end_quote++)
          {
            end_quote = PL_strchr(end_quote, '"');
            if (!end_quote || *(end_quote - 1) != '\\')
              break;
          }
          const char *mailbox = end_quote ? PL_strchr(end_quote, '<') : (char *)NULL;
          const char *comma = end_quote ? PL_strchr(end_quote, ',') : (char *)NULL;
          if (!mailbox || (comma && comma < mailbox))
          {
            leave_quotes = 1; /* no mailbox for this address */
            *addr_out++ = '\"';
          }
        }

        while (*line_end)
        {
          if (*line_end == '\\')
          {
            line_end++;
            if (paren_depth == 0 && (*line_end == '\\' || *line_end == '\"'))
              *addr_out++ = *line_end++;
            continue;
          }
          else if (*line_end == '\"')
          {
            line_end++;  /* remove close " */
            break;
          }

          if (paren_depth == 0)
            COPY_CHAR(addr_out, line_end);

          NEXT_CHAR(line_end);
        }
        if (leave_quotes) *addr_out++ = '\"';
        continue;
      }

      if (*line_end == '(')
      {
        if (paren_depth == 0)
          oparen = line_end;
        paren_depth++;
      }
      else if (*line_end == '<' && paren_depth == 0)
      {
        mailbox_start = line_end;
      }
      else if (*line_end == '>' && mailbox_start && paren_depth == 0)
      {
        mailbox_end = line_end;
      }
      else if (*line_end == ')' && paren_depth > 0)
      {
        paren_depth--;
        if (paren_depth == 0)
        {
          const char *s = oparen + 1;

          /* Copy the chars inside the parens onto the "name" buffer. */

          /* Push out some whitespace before the paren, if
                             * there is non-whitespace there already.
                             */
          if (name_out > name_start && !IS_SPACE(name_out [-1]))
            *name_out++ = ' ';

          /* Skip leading whitespace.
           */
          while (IS_SPACE(*s) && s < line_end)
            s++;

          while (s < line_end)
          {
            /* Strip out " within () unless backslashed
             */
            if (*s == '\"')
            {
              s++;
              continue;
            }

            if (*s == '\\') /* remove one \ */
              s++;

            if (IS_SPACE(*s) && name_out > name_start && IS_SPACE(name_out[-1]))
              /* collapse consecutive whitespace */;
            else
              COPY_CHAR(name_out, s);

            NEXT_CHAR(s);
          }
          oparen = 0;
        }
      }
      else if (*line_end == ':' && !mailbox_start && paren_depth == 0)
      {
        // We're now in group format.
        in_group = true;
        COPY_CHAR(addr_out, line_end);
      }
      else if (*line_end == ';' && in_group && paren_depth == 0)
      {
        in_group = false;
        COPY_CHAR(addr_out, line_end);
        // We've got to the end of a group, therefore just continue with the loop
        // so that we avoid moving onto the next char at the end of this cycle of
        // the loop.
        continue;
      }
      else
      {
        /* If we're not inside parens or a <mailbox>, tack this
         * on to the end of the addr_buf.
         */
        if (paren_depth == 0 && (!mailbox_start || mailbox_end))
        {
          /* Eat whitespace at the beginning of the line,
           * and eat consecutive whitespace within the line.
           */
          if (IS_SPACE(*line_end)
              && (addr_out == addr_start || IS_SPACE(addr_out[-1])))
            /* skip it */;
          else
            COPY_CHAR(addr_out, line_end);
        }
      }

      NEXT_CHAR(line_end);
    }

  /* Now we have extracted a single address from the comma-separated
     * list of addresses.  The characters have been divided among the
     * various buffers: the parts inside parens have been placed in the
     * name_buf, and everything else has been placed in the addr_buf.
     * Quoted strings and backslashed characters have been `expanded.'
     *
     * If there was a <mailbox> spec in it, we have remembered where it was.
     * Copy that on to the addr_buf, replacing what was there, and copy the
     * characters not inside <> onto the name_buf, replacing what is there
     * now (which was just the parenthesized parts.)  (And we need to do the
     * quote and backslash hacking again, since we're coming from the
     * original source.)
     *
     * Otherwise, we're already done - the addr_buf and name_buf contain
     * the right data already (de-quoted.)
     */
    if (mailbox_end)
    {
      const char *s;
      NS_ASSERTION(*mailbox_start == '<', "");
      NS_ASSERTION(*mailbox_end == '>', "");

      /* First, copy the name.
       */
      name_out = name_start;
      s = this_start;

      /* Skip leading whitespace.
       */
      while (IS_SPACE(*s) && s < mailbox_start)
        s++;

      /* Copy up to (not including) the <
       */
      while (s < mailbox_start)
      {
        if (*s == '\"' && !quote_names_p)
        {
          s++;
          continue;
        }
        if (*s == '\\' && !quote_names_p)
        {
          s++;
          if (s < mailbox_start && (*s == '\\' || *s == '\"'))
            *name_out++ = *s++;
          continue;
        }
        if (IS_SPACE(*s) && name_out > name_start && IS_SPACE(name_out[-1]))
          /* collapse consecutive whitespace */;
        else
          COPY_CHAR(name_out, s);

        NEXT_CHAR(s);
      }

      /* Push out one space.
       */
      TRIM_WHITESPACE(name_start, name_out, ' ');
      s = mailbox_end + 1;

      /* Skip whitespace after >
       */
      while (IS_SPACE(*s) && s < line_end)
        s++;

      /* Copy from just after > to the end.
       */
      while (s < line_end)
      {
        if (*s == '\"' && !quote_names_p)
        {
          s++;
          continue;
        }
        if (*s == '\\' && !quote_names_p)
        {
          s++;
          if (s  < line_end && (*s == '\\' || *s == '\"'))
            *name_out++ = *s++;
          continue;
        }
        if (IS_SPACE(*s) && name_out > name_start && IS_SPACE(name_out[-1]))
          /* collapse consecutive whitespace */;
        else
          COPY_CHAR(name_out, s);

        NEXT_CHAR(s);
      }

      TRIM_WHITESPACE(name_start, name_out, 0);

      /* Now, copy the address.
       */
      mailbox_start++;
      addr_out = addr_start;
      s = mailbox_start;

      /* Skip leading whitespace.
       */
      while (IS_SPACE(*s) && s < mailbox_end)
        s++;

      /* Copy up to (not including) the >
       */
      while (s < mailbox_end)
      {
        if (*s == '\"' && !quote_addrs_p)
        {
          s++;
          continue;
        }
        if (*s == '\\' && !quote_addrs_p)
        {
          s++;
          if (s < mailbox_end && (*s == '\\' || *s == '\"'))
            *addr_out++ = *s++;
          continue;
        }
        COPY_CHAR(addr_out, s);
        NEXT_CHAR(s);
      }

      TRIM_WHITESPACE(addr_start, addr_out, 0);
    }
    /* No component of <mailbox> form.
     */
    else
    {
      TRIM_WHITESPACE(addr_start, addr_out, 0);
      TRIM_WHITESPACE(name_start, name_out, 0);
    }

    /* Now re-quote the names and addresses if necessary.
     */
#ifdef BUG11892
    // **** jefft - we don't want and shouldn't to requtoe the name, this
    // violate the RFC822 spec
    if (quote_names_p && names)
    {
      int L = name_out - name_start - 1;
      L = msg_quote_phrase_or_addr(name_start, L, false);
      name_out = name_start + L + 1;
    }
#endif

    if (quote_addrs_p && addresses)
    {
      int L = addr_out - addr_start - 1;
      L = msg_quote_phrase_or_addr(addr_start, L, true);
      addr_out = addr_start + L + 1;
    }

    addr_count++;

    /* If we only want the first address, we can stop now.
     */
    if (first_only_p)
      break;

    if (*line_end)
      NEXT_CHAR(line_end);

    /* Skip over extra whitespace, commas or semicolons between addresses. */
    while (*line_end && (IS_SPACE(*line_end) || *line_end == ',' ||
                         *line_end == ';'))
      line_end++;

    this_start = line_end;
    name_start = name_out;
    addr_start = addr_out;
  }

  /* Make one more pass through and convert all whitespace characters
   * to SPC.  We could do that in the first pass, but this is simpler.
   */
  {
    char *s;
    for (s = name_buf; s < name_out; NEXT_CHAR(s))
      if (IS_SPACE(*s) && *s != ' ')
        *s = ' ';
    for (s = addr_buf; s < addr_out; NEXT_CHAR(s))
      if (IS_SPACE(*s) && *s != ' ')
        *s = ' ';
  }

  if (names)
    *names = name_buf;
  else
    PR_Free(name_buf);

  if (addresses)
    *addresses = addr_buf;
  else
    PR_Free(addr_buf);

  return addr_count;
}

/* msg_quote_phrase_or_addr
 *
 * Given a single mailbox, this quotes the characters in it which need
 * to be quoted; it writes into `address' and returns a new length.
 * `address' is assumed to be long enough; worst case, its size will
 * be (N*2)+2.
 */
static int
msg_quote_phrase_or_addr(char *address, int32_t length, bool addr_p)
{
  int quotable_count = 0, in_quote = 0;
  int unquotable_count = 0;
  int32_t new_length, full_length = length;
  char *in, *out, *orig_out, *atsign = NULL, *orig_address = address;
  bool user_quote = false;
  bool quote_all = false;

  /* If the entire address is quoted, fall out now. */
  if (address[0] == '\"' && address[length - 1] == '\"')
     return length;

  /* Check to see if there is a routing prefix.  If there is one, we can
   * skip quoting it because by definition it can't need to be quoted.
   */
  if (addr_p && *address && *address == '@')
  {
    for (in = address; *in; NEXT_CHAR(in))
    {
      if (*in == ':')
      {
        length -= ++in - address;
        address = in;
        break;
      }
      else if (!IS_DIGIT(*in) && !IS_ALPHA(*in) && *in != '@' && *in != '.')
        break;
    }
  }

    for (in = address; in < address + length; NEXT_CHAR(in))
    {
        if (*in == 0)
            return full_length; /* #### horrible kludge... */

        else if (*in == '@' && addr_p && !atsign && !in_quote)
    {
            /* Exactly one unquoted at-sign is allowed in an address. */
      if (atsign)
        quotable_count++;
            atsign = in;

      /* If address is of the form '"userid"@somewhere.com' don't quote
       * the quotes around 'userid'.  Also reset the quotable count, since
       * any quotables we've seen are already inside quotes.
       */
      if (address[0] == '\"' && in > address + 2 && *(in - 1) == '\"' && *(in - 2) != '\\')
        unquotable_count -= 2, quotable_count = 0, user_quote = true;
    }

        else if (*in == '\\')
        {
            if (in + 1 < address + length && (*(in + 1) == '\\' || *(in + 1) == '\"'))
                /* If the next character is a backslash or quote, this backslash */
                /* is an escape backslash; ignore it and the next character.     */
                in++;
            else
                /* If the name contains backslashes or quotes, they must be escaped. */
                unquotable_count++;
        }

        else if (*in == '\"')
            /* If the name contains quotes, they must be escaped. */
            unquotable_count++, in_quote = !in_quote;

        else if (  /* *in >= 127 || *in < 0  ducarroz: 8bits characters will be mime encoded therefore they are not a problem
             ||*/ (*in == ';' && !addr_p) || *in == '$' || *in == '(' || *in == ')'
                 || *in == '<' || *in == '>' || *in == '@' || *in == ',')
            /* If the name contains control chars or Header specials, it needs to
             * be enclosed in quotes.  Double-quotes and backslashes will be dealt
             * with separately.
             *
             * The ":" character is explicitly not in this list, though Header says
             * it should be quoted, because that has been seen to break VMS
             * systems.  (Rather, it has been seen that there are Unix SMTP servers
             * which accept RCPT TO:<host::user> but not RCPT TO:<"host::user"> or
             * RCPT TO:<host\:\:user>, which is the syntax that VMS/DECNET hosts
             * use.
             *
             * For future reference: it is also claimed that some VMS SMTP servers
             * allow \ quoting but not "" quoting; and that sendmail uses self-
             * contradcitory quoting conventions that violate both RFCs 821 and
             * 822, so any address quoting on a sendmail system will lose badly.
             *
             * The ";" character in an address is a group delimiter, therefore it
             * should not be quoted in that case.
             */
      quotable_count++;

    else if (!atsign && (*in == '[' || *in == ']'))
      /* Braces are normally special characters, except when they're
       * used for domain literals (e.g. johndoe@[127.0.0.1].acme.com).
       */
      quotable_count++;

        else if (addr_p && *in == ' ')
            /* Naked spaces are allowed in names, but not addresses. */
            quotable_count++;

        else if (   !addr_p
             && (*in == '.' || *in == '!' || *in == '$' || *in == '%'))
            /* Naked dots are allowed in addresses, but not in names.
             * The other characters (!$%) are technically allowed in names, but
             * are surely going to cause someone trouble, so we quote them anyway.
             */
            quotable_count++;
    }

    if (quotable_count == 0 && unquotable_count == 0)
        return full_length;

  /* We must quote the entire string if there are quotables outside the user
   * quote.
   */
  if (!atsign || (user_quote && quotable_count > 0))
    quote_all = true, atsign = NULL;

    /* Add 2 to the length for the quotes, plus one for each character
     * which will need a backslash, plus one for a null terminator.
     */
    new_length = length + quotable_count + unquotable_count + 3;

    in = address;
    out = orig_out = (char *)PR_Malloc(new_length);
  if (!out)
  {
    *orig_address = 0;
    return 0;
  }

  /* Start off with a quote.
   */
  *out++ = '\"';

  while (*in)
  {
    if (*in == '@')
    {
      if (atsign == in)
        *out++ = '\"';
      *out++ = *in++;
      continue;
    }
        else if (*in == '\"')
    {
      if (!user_quote || (in != address && in != atsign - 1))
        *out++ = '\\';
      *out++ = *in++;
      continue;
    }
        else if (*in == '\\')
        {
        if (*(in + 1) == '\\' || *(in + 1) == '\"')
          *out++ = *in++;
      else
                *out++ = '\\';
      *out++ = *in++;
      continue;
        }
    else
      COPY_CHAR(out, in);

    NEXT_CHAR(in);
  }

  /* Add a final quote if we are quoting the entire string.
   */
  if (quote_all)
    *out++ = '\"';
  *out++ = 0;

  NS_ASSERTION(new_length >= (out - orig_out), "miscalculated quoted length");
  memcpy(address, orig_out, new_length);
  PR_Free(orig_out); /* make sure we release the string we allocated */

  // Return how many bytes we wrote, not counting the null byte.
  return out - orig_out - 1;
}

/* msg_unquote_phrase_or_addr
 *
 * Given a name or address that might have been quoted
 * it will take out the escape and double quotes
 * The caller is responsible for freeing the resulting
 * string.
 */
static nsresult
msg_unquote_phrase_or_addr(const char *line, bool preserveIntegrity, char **lineout)
{
  if (!line || !lineout)
    return NS_OK;

  /* If the first character isn't a double quote, there is nothing to do
   */
  if (*line != '\"')
  {
    *lineout = strdup(line);
    if (!*lineout)
      return NS_ERROR_OUT_OF_MEMORY;
    else
      return NS_OK;
  }

  /* in preserveIntegrity mode, we must preserve the quote if the name contains a comma */
  if (preserveIntegrity)
  {
    const char * open_quote = nullptr;
    const char * comma = nullptr;;
    const char * at_sign = nullptr;
    const char * readPos = line + 1;

    while (*readPos)
    {
      if (*readPos == ',')
      {
        if (!open_quote)
          comma = readPos;
      }
      else if (*readPos == '@')
      {
        at_sign = readPos;
        break;
      }
      else if (*readPos == '"')
      {
        if (!open_quote)
          open_quote = readPos;
        else
          open_quote = nullptr;
      }

      readPos ++;
    }

    if (comma && at_sign)
    {
      *lineout = strdup(line);
      if (!*lineout)
        return NS_ERROR_OUT_OF_MEMORY;
      else
        return NS_OK;
    }
  }

  /* Don't copy the first double quote
   */
  *lineout = strdup(line + 1);
  if (!*lineout)
    return NS_ERROR_OUT_OF_MEMORY;

  const char *lineptr = line + 1;
  char *outptr = *lineout;
  bool escaped = false;

  while (*lineptr)
  {
    /* If the character is an '\' then output the character that was
     * escaped.  If it was part of the quote then don't output it.
     */
    if (*lineptr == '\\')
    {
      escaped = true;
      lineptr++;
    }
    if (*lineptr == '\"' && !escaped)
      lineptr++;
    escaped = false;

    if (*lineptr)
    {
      COPY_CHAR(outptr, lineptr);
      NEXT_CHAR(lineptr);
    }
  }
  *outptr = '\0';

  return NS_OK;
}

/**
 * Given a string which contains a list of Header addresses, returns a
 * comma-separated list of just the `mailbox' portions.
 */
NS_IMETHODIMP
nsMsgHeaderParser::ExtractHeaderAddressMailboxes(const nsACString &aLine,
                                                 nsACString &aResult)
{
  if (aLine.IsEmpty())
  {
    aResult.Truncate();
    return NS_OK;
  }

  char *addrs = 0;
  int status = msg_parse_Header_addresses(PromiseFlatCString(aLine).get(),
                                          NULL, &addrs);
  if (status <= 0)
    return NS_ERROR_OUT_OF_MEMORY;

  char *s = addrs;
  uint32_t i, size = 0;

  for (i = 0; (int) i < status; i++)
  {
    uint32_t j = strlen(s);
    s += j + 1;
    size += j;
    if ((int)(i + 1) < status)
      size += 2;
  }

  nsCString result;
  result.SetLength(size);
  s = addrs;
  char* out = result.BeginWriting();
  for (i = 0; (int)i < status; i++)
  {
    uint32_t j = strlen(s);
    memcpy(out, s, j);
    out += j;
    if ((int)(i+1) < status)
    {
      *out++ = ',';
      *out++ = ' ';
    }
    s += j + 1;
  }

  PR_Free(addrs);
  aResult = result;
  return NS_OK;
}


/**
 * Given a string which contains a list of Header addresses, returns a
 * comma-separated list of just the `user name' portions.  If any of
 * the addresses doesn't have a name, then the mailbox is used instead.
 *
 * The names are *unquoted* and therefore cannot be re-parsed in any way.
 * They are, however, nice and human-readable.
 */
NS_IMETHODIMP
nsMsgHeaderParser::ExtractHeaderAddressNames(const nsACString &aLine,
                                             nsACString &aResult)
{
  if (aLine.IsEmpty())
  {
    aResult.Truncate();
    return NS_OK;
  }

  char *names = 0;
  char *addrs = 0;
  int status = msg_parse_Header_addresses(PromiseFlatCString(aLine).get(),
                                          &names, &addrs);
  if (status <= 0)
    return NS_ERROR_FAILURE;

  uint32_t len1, len2;
  char *s1, *s2, *out;
  uint32_t i, size = 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int)i < status; i++)
  {
    len1 = strlen(s1);
    len2 = strlen(s2);
    s1 += len1 + 1;
    s2 += len2 + 1;
    size += (len1 ? len1 : len2);
    if ((int)(i + 1) < status)
      size += 2;
  }

  nsCString result;
  result.SetLength(size);

  out = result.BeginWriting();
  s1 = names;
  s2 = addrs;
  for (i = 0; (int)i < status; i++)
  {
    len1 = strlen(s1);
    len2 = strlen(s2);

    if (len1)
    {
      memcpy(out, s1, len1);
      out += len1;
    }
    else
    {
      memcpy(out, s2, len2);
      out += len2;
    }

    if ((int)(i + 1) < status)
    {
      *out++ = ',';
      *out++ = ' ';
    }
    s1 += len1 + 1;
    s2 += len2 + 1;
  }

  PR_Free(names);
  PR_Free(addrs);
  aResult = result;
  return NS_OK;
}

/* msg_extract_Header_address_name
 *
 * Like MSG_ExtractHeaderAddressNames(), but only returns the first name
 * in the list, if there is more than one.
 */
NS_IMETHODIMP
nsMsgHeaderParser::ExtractHeaderAddressName(const nsACString &aLine,
                                            nsACString &aResult)
{
  if (aLine.IsEmpty())
  {
    aResult.Truncate();
    return NS_OK;
  }

  char *name = 0;
  char *addr = 0;
  int status = msg_parse_Header_addresses(PromiseFlatCString(aLine).get(),
                                          &name, &addr, false, false,
                                          true);
  if (status <= 0)
    return NS_ERROR_FAILURE;

  /* This can happen if there is an address like "From: foo bar" which
   * we parse as two addresses (that's a syntax error.)  In that case,
   * we'll return just the first one (the rest is after the NULL.)
   *
   * NS_ASSERTION(status == 1);
   */
  aResult = (name && *name) ? name : addr;

  PR_Free(name);
  PR_Free(addr);
  return NS_OK;
}

/* msg_format_Header_addresses
 */
static char *
msg_format_Header_addresses (const char *names, const char *addrs,
               int count, bool wrap_lines_p)
{
  char *result, *out;
  const char *s1, *s2;
  uint32_t i, size = 0;
  uint32_t column = 10;
  uint32_t len1, len2;
  uint32_t name_maxlen = 0;
  uint32_t addr_maxlen = 0;

  if (count <= 0)
    return 0;

  s1 = names;
  s2 = addrs;
  for (i = 0; (int)i < count; i++)
  {
    len1 = strlen(s1);
    len2 = strlen(s2);
    s1 += len1 + 1;
    s2 += len2 + 1;

    len1 = (len1 * 2) + 2;  //(N*2) + 2 in case we need to quote it
    len2 = (len2 * 2) + 2;
    name_maxlen = std::max(name_maxlen, len1);
    addr_maxlen = std::max(addr_maxlen, len2);
    size += len1 + len2 + 10;
  }

  result = (char *)PR_Malloc(size + 1);
  char *aName = (char *)PR_Malloc(name_maxlen + 1);
  char *anAddr = (char *)PR_Malloc(addr_maxlen + 1);
  if (!result || !aName || !anAddr)
  {
    PR_FREEIF(result);
    PR_FREEIF(aName);
    PR_FREEIF(anAddr);
    return 0;
  }

  out = result;
  s1 = names;
  s2 = addrs;

  for (i = 0; (int)i < count; i++)
  {
    char *o;

    PL_strncpy(aName, s1, name_maxlen);
    PL_strncpy(anAddr, s2, addr_maxlen);
    len1 = msg_quote_phrase_or_addr(aName, strlen(s1), false);
    len2 = msg_quote_phrase_or_addr(anAddr, strlen(s2), true);

    if (   wrap_lines_p && i > 0
        && (column + len1 + len2 + 3 + (((int)(i+1) < count) ? 2 : 0) > 76))
    {
      if (out > result && out[-1] == ' ')
        out--;
      *out++ = '\r';
      *out++ = '\n';
      *out++ = '\t';
      column = 8;
    }

    o = out;

    if (len1)
    {
      memcpy(out, aName, len1);
      out += len1;
      *out++ = ' ';
      *out++ = '<';
    }
    memcpy(out, anAddr, len2);
    out += len2;
    if (len1)
      *out++ = '>';

    if ((int)(i+1) < count)
    {
      *out++ = ',';
      *out++ = ' ';
    }
    s1 += strlen(s1) + 1;
    s2 += strlen(s2) + 1;

    column += (out - o);
  }
  *out = 0;

  PR_FREEIF(aName);
  PR_FREEIF(anAddr);

  return result;
}

/* msg_remove_duplicate_addresses
 *
 * Returns a copy of ADDRS which may have had some addresses removed.
 * Addresses are removed if they are already in either ADDRS or OTHER_ADDRS.
 * (If OTHER_ADDRS contain addresses which are not in ADDRS, they are not
 * added.  That argument is for passing in addresses that were already
 * mentioned in other header fields.)
 *
 * Addresses are considered to be the same if they contain the same mailbox
 * part (case-insensitive.)  Real names and other comments are not compared.
 */
static char *
msg_remove_duplicate_addresses(const nsACString &addrs,
                               const nsACString &other_addrs)
{
  /* This is probably way more complicated than it should be... */
  char *s1 = 0, *s2 = 0;
  char *output = 0, *out = 0;
  char *result = 0;
  int count1 = 0, count2 = 0, count3 = 0;
  int size3 = 0;
  char *names1 = 0, *names2 = 0;
  char *addrs1 = 0, *addrs2 = 0;
  char **a_array1 = 0, **a_array2 = 0, **a_array3 = 0;
  char **n_array1 = 0,                 **n_array3 = 0;
  int i, j;
  uint32_t addedlen = 0;

  count1 = msg_parse_Header_addresses(nsCString(addrs).get(), &names1, &addrs1);
  if (count1 < 0) goto FAIL;
  if (count1 == 0)
  {
    result = strdup("");
    goto FAIL;
  }
  count2 = msg_parse_Header_addresses(nsCString(other_addrs).get(), &names2, &addrs2);
  if (count2 < 0) goto FAIL;

  a_array1 = (char **)PR_Malloc(count1 * sizeof(char *));
  if (!a_array1) goto FAIL;
  n_array1 = (char **)PR_Malloc(count1 * sizeof(char *));
  if (!n_array1) goto FAIL;

  if (count2 > 0)
  {
    a_array2 = (char **)PR_Malloc(count2 * sizeof(char *));
    if (!a_array2) goto FAIL;
    /* don't need an n_array2 */
  }

  a_array3 = (char **)PR_Malloc(count1 * sizeof(char *));
  if (!a_array3) goto FAIL;
  n_array3 = (char **)PR_Malloc(count1 * sizeof(char *));
  if (!n_array3) goto FAIL;


  /* fill in the input arrays */
  s1 = names1;
  s2 = addrs1;
  for (i = 0; i < count1; i++)
  {
    n_array1[i] = s1;
    a_array1[i] = s2;
    s1 += strlen(s1) + 1;
    s2 += strlen(s2) + 1;
  }

  s2 = addrs2;
  for (i = 0; i < count2; i++)
  {
    a_array2[i] = s2;
    s2 += strlen(s2) + 1;
  }

  /* Iterate over all addrs in the "1" arrays.
   * If those addrs are not present in "3" or "2", add them to "3".
   */
  for (i = 0; i < count1; i++)
  {
    bool found = false;
    for (j = 0; j < count2; j++)
      if (!PL_strcasecmp (a_array1[i], a_array2[j]))
      {
        found = true;
        break;
      }

    if (!found)
      for (j = 0; j < count3; j++)
        if (!PL_strcasecmp(a_array1[i], a_array3[j]))
        {
          found = true;
          break;
        }

    if (!found)
    {
      n_array3[count3] = n_array1[i];
      a_array3[count3] = a_array1[i];
      size3 += (strlen(n_array3[count3]) + strlen(a_array3[count3]) + 10);
      count3++;
      NS_ASSERTION (count3 <= count1, "");
      if (count3 > count1) break;
    }
  }
  
  uint32_t outlen;
  outlen = size3 + 1;
  output = (char *)PR_Malloc(outlen);
  if (!output) goto FAIL;

  *output = 0;
  out = output;
  s2 = output;
  for (i = 0; i < count3; i++)
  {
    PL_strncpyz(out, a_array3[i], outlen);
    addedlen = strlen(out);
    outlen -= addedlen;
    out += addedlen;
    *out++ = 0;
  }
  s1 = out;
  for (i = 0; i < count3; i++)
  {
    PL_strncpyz(out, n_array3[i], outlen);
    addedlen = strlen(out);
    outlen -= addedlen;
    out += addedlen;
    *out++ = 0;
  }
  result = msg_format_Header_addresses(s1, s2, count3, false);

 FAIL:
  FREEIF(a_array1);
  FREEIF(a_array2);
  FREEIF(a_array3);
  FREEIF(n_array1);
  FREEIF(n_array3);
  FREEIF(names1);
  FREEIF(names2);
  FREEIF(addrs1);
  FREEIF(addrs2);
  FREEIF(output);
  return result;
}


/* msg_make_full_address
 *
 * Given an e-mail address and a person's name, cons them together into a
 * single string of the form "name <address>", doing all the necessary quoting.
 * A new string is returned, which you must free when you're done with it.
 */
static char *
msg_make_full_address(const char* name, const char* addr)
{
  int nl = name ? strlen (name) : 0;
  int al = addr ? strlen (addr) : 0;
  char *buf, *s;
  uint32_t buflen, slen;
  int L;
  if (al == 0)
    return 0;

  buflen = (nl * 2) + (al * 2) + 25;
  buf = (char *)PR_Malloc(buflen);
  if (!buf)
    return 0;
  if (nl > 0)
  {
    PL_strncpyz(buf, name, buflen);
    L = msg_quote_phrase_or_addr(buf, nl, false);
    s = buf + L;
    slen = buflen - L;
    if ( slen > 2 ) {
        *s++ = ' ';
        *s++ = '<';
        slen -= 2; // for ' ' and '<'
    }
  }
  else
  {
    s = buf;
    slen = buflen;
  }

  PL_strncpyz(s, addr, slen);
  L = msg_quote_phrase_or_addr(s, al, true);
  s += L;
  if (nl > 0)
    *s++ = '>';
  *s = 0;
  L = (s - buf) + 1;
  buf = (char *)PR_Realloc (buf, L);
  return buf;
}

MsgAddressObject::MsgAddressObject(const nsAString &aName,
    const nsAString &aEmail)
: mName(aName),
  mEmail(aEmail)
{
}

NS_IMPL_ISUPPORTS1(MsgAddressObject, msgIAddressObject)

NS_IMETHODIMP MsgAddressObject::GetName(nsAString &name)
{
  name = mName;
  return NS_OK;
}

NS_IMETHODIMP MsgAddressObject::GetEmail(nsAString &email)
{
  email = mEmail;
  return NS_OK;
}

NS_IMETHODIMP MsgAddressObject::GetGroup(JS::MutableHandleValue members)
{
  members.set(JS::UndefinedValue());
  return NS_OK;
}

NS_IMETHODIMP MsgAddressObject::ToString(nsAString &display)
{
  nsMsgHeaderParser headerParser;
  nsAutoString quotedString;
  headerParser.MakeMimeAddress(mName, mEmail, quotedString);
  nsString displayAddr;
  headerParser.UnquotePhraseOrAddrWString(quotedString.get(), false,
    getter_Copies(displayAddr));
  display = displayAddr;

  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::MakeMailboxObject(const nsAString &aName,
    const nsAString &aEmail, msgIAddressObject **retval)
{
  nsCOMPtr<msgIAddressObject> object = new MsgAddressObject(aName, aEmail);
  object.forget(retval);
  return NS_OK;
}

static MsgAddressObject *MakeSingleAddress(
    const nsAString &aDisplay)
{
  // This is a wasteful copy, but the internal API does not have RFindChar on
  // nsAString, only nsString.
  nsString display(aDisplay);
  // Strip leading and trailing whitespace.
  display.Trim(kWhitespace, true, true);
  nsCOMPtr<msgIAddressObject> object;
  int32_t addrstart = display.RFindChar('<');
  if (addrstart != -1)
  {
    // Adjust is used to strip off exactly one space char if it's present.
    int32_t adjust = addrstart == 0 ? 0 : 1;
    int32_t addrend = display.RFindChar('>');
    return new MsgAddressObject(
      StringHead(display, addrstart - adjust),
      Substring(display, addrstart + 1, addrend - addrstart - 1));
  }
  else
  {
    return new MsgAddressObject(EmptyString(), display);
  }
}

NS_IMETHODIMP nsMsgHeaderParser::MakeFromDisplayAddress(
    const nsAString &aDisplay, uint32_t *count, msgIAddressObject ***retval)
{
  // We split on every comma, so long as a @ exists before that comma.
  nsCOMArray<msgIAddressObject> addresses;
  int32_t lastComma = -1;
  while (!aDisplay.IsEmpty() && lastComma < (int32_t)aDisplay.Length())
  {
    // Find the next , that follows an email address (which must have an @).
    int32_t atSign = aDisplay.FindChar('@', lastComma + 1);
    // If there is no @, just consume the rest of the string as the "address"
    if (atSign == -1)
      atSign = aDisplay.Length() - 1;
    int32_t nextComma = aDisplay.FindChar(',', atSign + 1);
    if (nextComma == -1)
      nextComma = aDisplay.Length();

    // The substring from [lastComma + 1, nextComma) is an email address.
    addresses.AppendElement(MakeSingleAddress(
      Substring(aDisplay, lastComma + 1, nextComma - (lastComma + 1))));
    
    // Move lastComma along
    lastComma = nextComma;
  }

  // Add all the elements to the output
  msgIAddressObject **out = (msgIAddressObject **)NS_Alloc(
    sizeof(msgIAddressObject*) * addresses.Length());
  for (uint32_t i = 0; i < addresses.Length(); i++)
    NS_IF_ADDREF(out[i] = addresses[i]);

  *count = addresses.Length();
  *retval = out;
  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::ParseEncodedHeader(const nsACString &aHeader,
    const char *aCharset, bool preserveGroups, uint32_t *length,
    msgIAddressObject ***retval)
{
  NS_ENSURE_ARG_POINTER(length);
  NS_ENSURE_ARG_POINTER(retval);

  // We are not going to be implementing group parsing yet.
  if (preserveGroups)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsIMimeConverter> converter = mozilla::services::GetMimeConverter();

  nsCString nameBlob, emailBlob;
  uint32_t count;
  nsresult rv = ParseHeaderAddresses(PromiseFlatCString(aHeader).get(),
    getter_Copies(nameBlob), getter_Copies(emailBlob), &count);
  NS_ENSURE_SUCCESS(rv, rv);

  msgIAddressObject **addresses = static_cast<msgIAddressObject**>(NS_Alloc(
    sizeof(msgIAddressObject*) * count));

  // The contract of ParseHeaderAddresses sucks: it's \0-delimited strings
  const char *namePtr = nameBlob.get();
  const char *emailPtr = emailBlob.get();
  for (uint32_t i = 0; i < count; i++)
  {
    nsCString clean;
    nsString utf16Name;
    UnquotePhraseOrAddr(namePtr, false, getter_Copies(clean));
    converter->DecodeMimeHeader(clean.get(), aCharset, false, true, utf16Name);
    addresses[i] = new MsgAddressObject(utf16Name,
      NS_ConvertUTF8toUTF16(emailPtr));
    NS_ADDREF(addresses[i]);

    // Go past the \0 to the next one
    namePtr += strlen(namePtr) + 1;
    emailPtr += strlen(emailPtr) + 1;
  }

  *length = count;
  *retval = addresses;
  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::ParseDecodedHeader(const nsAString &aHeader,
    bool preserveGroups, uint32_t *length, msgIAddressObject ***retval)
{
  NS_ENSURE_ARG_POINTER(length);
  NS_ENSURE_ARG_POINTER(retval);

  // We are not going to be implementing group parsing yet.
  if (preserveGroups)
    return NS_ERROR_NOT_IMPLEMENTED;

  char16_t **rawNames = nullptr;
  char16_t **rawEmails = nullptr;
  char16_t **rawFull = nullptr;
  uint32_t count;
  nsresult rv = ParseHeadersWithArray(PromiseFlatString(aHeader).get(),
    &rawEmails, &rawNames, &rawFull, &count);
  NS_ENSURE_SUCCESS(rv, rv);

  msgIAddressObject **addresses = static_cast<msgIAddressObject**>(NS_Alloc(
    sizeof(msgIAddressObject*) * count));

  for (uint32_t i = 0; i < count; i++)
  {
    nsString clean;
    UnquotePhraseOrAddrWString(rawNames[i], false, getter_Copies(clean));
    addresses[i] = new MsgAddressObject(clean, nsDependentString(rawEmails[i]));
    NS_ADDREF(addresses[i]);
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawNames);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawEmails);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, rawFull);

  *length = count;
  *retval = addresses;
  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::MakeMimeHeader(msgIAddressObject **addresses,
    uint32_t length, nsAString &header)
{
  NS_ENSURE_ARG(addresses);

  header.Truncate();
  nsAutoString name, email, temp;
  for (uint32_t i = 0; i < length; i++)
  {
    addresses[i]->GetName(name);
    addresses[i]->GetEmail(email);
    MakeMimeAddress(name, email, temp);
    if (!header.IsEmpty())
      header.AppendLiteral(", ");
    header += temp;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::ExtractFirstName(const nsAString &header,
    nsAString &retval)
{
  using namespace mozilla::mailnews;
  ExtractName(DecodedHeader(header), retval);
  return NS_OK;
}

NS_IMETHODIMP nsMsgHeaderParser::MakeGroupObject(const nsAString &name,
    msgIAddressObject **members, uint32_t length, msgIAddressObject **retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

