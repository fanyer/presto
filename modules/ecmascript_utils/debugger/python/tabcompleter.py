from re import compile
from examiner import Examiner

re_identifier = "[A-Za-z$_][-0-9A-Za-z$_]*"
re_command = compile("^[a-z]+$")
re_property = compile("^([a-z]+)(/[-a-z*+?]+)? (.*[^-0-9A-Za-z$_])?(" + re_identifier + ")$")

re_identifier_at_end = compile("^(.*[^-0-9A-Za-z$_])?(" + re_identifier + ")$")
re_whitespace_at_end = compile("^(.*[^ ]+)?([ ]+)$")

operators3 = ["<<=", ">>="]
operators2 = ["<<", ">>", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "||", "&&", "|=", "&="]
operators1 = "+-*/%<>.,&|"

class TabCompletionCandidates:
    def __init__(self, candidates): self.candidates = candidates

class TabCompleter:
    def __init__(self, debugger):
        self.__debugger = debugger

    def __complete(self, buffer, cursor, replaceLength, replaceWith):
        buffer = buffer[:cursor - replaceLength] + replaceWith + buffer[cursor:]
        cursor = cursor - replaceLength + len(replaceWith)
        return True, buffer, cursor

    def __findCompletion(self, buffer, cursor, replaced, candidates, showCandidates, addSpace):
        candidates = filter(lambda name: name.startswith(replaced), candidates)
        if len(candidates) == 0: return (False, buffer, cursor)
        else:
            perfect = candidates[0]
            for name in candidates[1:]:
                if name != perfect: break
            else: return self.__complete(buffer, cursor, len(replaced), addSpace and perfect + " " or perfect)

            longest = replaced
            first = True
            for name in candidates:
                if first:
                    longest = name
                    first = False
                elif not name.startswith(longest):
                    longestBefore = longest
                    for index in range(len(replaced), len(longest)):
                        if name.startswith(longestBefore[:index]): longest = longestBefore[:index]
                        else: break
            if longest != replaced: return self.__complete(buffer, cursor, len(replaced), longest)
            elif showCandidates:
                names = {}
                for name in candidates: names[name] = True
                names = names.keys()
                names.sort()
                raise TabCompletionCandidates, names
        return (False, buffer, cursor)

    def __argumentIsScript(self, buffer):
        from debugger import DebuggerError
        try: return self.__debugger.getCommandInvoker(buffer).getName() in ("print", "examine", "break")
        except DebuggerError: return False

    def __call__(self, buffer, cursor, showCandidates):
        failure = (False, buffer, cursor)

        if not buffer or cursor == 0: return failure

        before = buffer[:cursor]
        after = buffer[cursor:]

        if re_command.match(before):
            return self.__findCompletion(buffer, cursor, before, self.__debugger.getCommandNames(), showCandidates, True)

        runtime = self.__debugger.getCurrentRuntime()
        match = re_property.match(before)
        if runtime and match and self.__argumentIsScript(buffer):
            command = match.group(1)
            expression = match.group(3)
            identifier = match.group(4)

            inScope = True
            base = ""
            parens = []

            if expression and expression.strip()[-1] == '.':
                expression = expression.strip()[:-1]
                inScope = False

                while expression:
                    match = re_whitespace_at_end.match(expression)
                    if match:
                        base = match.group(2) + base
                        expression = expression[:match.start(2)]
                        
                    if not expression: break

                    match = re_identifier_at_end.match(expression)
                    if match:
                        base = match.group(2) + base
                        expression = expression[:match.start(2)]
                        continue

                    ch = expression[-1]
                    if ch in (')', ']'):
                        parens.append({ ')': '(', ']': '[' }[ch])
                        base = ch + base
                        expression = expression[:-1]
                        continue

                    if ch in ('(', '['):
                        if not parens or parens[-1] != ch: break
                        parens.pop(-1)
                        base = ch + base
                        expression = expression[:-1]
                        continue

                    if expression[-1] == '.':
                        base = '.' + base
                        expression = expression[:-1]
                        continue

                    if len(expression) >= 3 and expression[-3:] in operators3:
                        if not parens: break
                        base = expression[-3:] + base
                        expression = expression[:-3]
                        continue

                    if len(expression) >= 2 and expression[-2:] in operators2:
                        if not parens: break
                        base = expression[-2:] + base
                        expression = expression[:-2]
                        continue

                    if len(expression) >= 1 and expression[-1:] in operators1:
                        if not parens: break
                        base = expression[-1:] + base
                        expression = expression[:-1]
                        continue

                    return failure

            if not parens:
                if inScope:
                    object = None
                else:
                    try: result = runtime.eval(base, True, True)
                    except KeyboardInterrupt: return failure

                    if result and result.isObject(): object = result.getValue()
                    else: return failure

                examiner = Examiner(runtime, object, 0xffff, 0, False, False, [], inScope)

                try: examiner.update()
                except KeyboardInterrupt: return failure

                if inScope: scope = examiner.getScope()
                else: scope = [object]

                propertyNames = []

                for object in scope:
                    while object:
                        propertyNames.extend([name for name, value in examiner.getObjectProperties(object)])
                        object = object.getPrototype()

                return self.__findCompletion(buffer, cursor, identifier, propertyNames, showCandidates, False)

        return failure
