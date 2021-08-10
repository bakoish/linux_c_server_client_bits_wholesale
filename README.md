# linux_c_server_client_bits_wholesale

Projekt z programowania w systemie Linux napisany w jezyku c++. 

Przygorowałem dwa programy, producenta i konsumenta, współpracujące ze
sobą w układzie: jeden producent wielu konsumentów. Producent musi być gotowy
na równoczesną obsługę nawet setek klientów.
Producent „produkuje bajty” i umieszcza je w „magazynie”. Zawartość magazynu
jest udostępniania konsumentom za pomocą protokołu TCP/IP.

Pełen opis w wymaganiach projektu.
