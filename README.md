# Flex4Apps Log Analysis Toolkit - FlexLogTemplater

This toolkit contains components to analyze unstructured log messages. Its
central functionality is the extraction of so-called log templates from
unstructured log messages, a process often termed *log parsing*. With the help
of these templates, unstructured log messages may be converted to structured
ones, allowing for further analysis relying on information such as the frequency
of log message types and parameter values.

## Log Templater

The functionality of the log templater is best described by example. The
following log messages

```
Directory /home/user_x/dir_a was successfully created
Directory /home/user_y/dir_b was successfully created
Login of user A, status: successful
Login of user B, status: failed
```

will be converted by the log templater to the templates

```
Directory <*> was successfully created
Login of user <*>, status: <*>
```

### Online Parameter Detection

The templater is based on the layered structure of the log parser DRAIN [^1].
Log messages are first sorted by their length in terms of individual tokens,
than by their first or last token. In these sorted log messages, differing
tokens are then detected as parameters and replaced by wildcards, up to a
certain user-definable threshold, which sets the maximum ratio of parameters to
tokens. For details, see DRAIN [^1] and
[online_templater.hpp](flt/templating/online_templater.hpp).

In the example above, the log messages would be sorted as follows:

```
[Length: 5, token: 'Directory'] Directory /home/user_x/dir_a was successfully created
[Length: 5, token: 'Directory'] Directory /home/user_y/dir_b was successfully created
[Length: 6, token: 'Login'] Login of user A, status: successful
[Length: 6, token: 'Login'] Login of user B, status: failed
```

For the first two messages, the threshold must be set >= 1 parameter / 5 tokens
= 0.2 to detect the parameter. For the last two messages, it must be set >= 2 /
6 = 0.33.

This part of the templater works online, on a message-by-message basis, thus not
relying on information obtained from a larger number of messages such as
frequencies of specific tokens.

### Offline Correction by Bijection Detection

The above parameter detection is very sensitive to the threshold. Setting it too
low may result in not all parameters being detected, while setting it too high
may lead to non-parameter tokens being falsely detected as parameters.

To decrease the sensitivity of the parameter detection on the threshold, a
corrective algorithm is introduced, which is inspired by the bijection step of
the log parser IPLoM [^2]. This algorithm detects bijections at the token
positions detected as parameters and corrects the log templates by restoring the
original tokens at the token positions where bijections were found.

Again, here is an example to illustrate the process. The log messages
```
User A logged on successfully
User B logged on successfully
User C saved state successfully
User D saved state successfully
```

are first converted to the log template

```
User <*> <*> <*> successfully
```

Then, a bijection is found for positions 3 and 4: `logged` always appears with
`on`, and `saved` always appears with `state`. Consequently, after the bijection
detection, the above template is split to

```
User <*> logged on successfully
User <*> saved state successfully
```

For the bijection detection to work, parameter values need to be stored for
several log messages. Consequently, this corrective step does not work on a
message-by-message basis and thus is performed offline.

The combination of a layered structure with a bijection detection leads to a
robust parameter detection.

## Getting Started with Templating

Clone the repository and cd into it, then

```
mkdir build
cd build
cmake ..
cmake --build .
./tools/online_templater ../log_example.txt ../templates_example.txt
```

The templater stores templates in `templates_example.txt` from log messages in
`log_example.txt`.

---

This toolkit was developed within the [ITEA](https://itea3.org/) project
[Flex4Apps](https://www.flex4apps-itea3.org/) by [Fraunhofer
SCAI](https://www.scai.fraunhofer.de/en/business-research-areas/numerical-data-driven-prediction.html).
Best practices on how to format log files to ease their analysis can be found
[here](https://f4a.readthedocs.io/en/latest/chapter02_stateOfTheArt/_structure.html#log-file-analysis).

[^1]: He, P., Zhu, J., Zheng, Z., & Lyu, M. R. (2017). Drain: an online log
parsing approach with fixed depth tree. In , Web Services (ICWS), 2017 IEEE
International Conference on (pp. 33–40). : IEEE.

[^2]: Makanju, A., Zincir-Heywood, A., & Milios, E. (2009). Clustering event
logs using iterative partitioning. In , Proceedings of the 15th ACM SIGKDD
international conference on knowledge discovery and data mining (pp. 1255–1264).
: ACM.
