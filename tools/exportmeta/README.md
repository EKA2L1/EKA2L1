## exportmeta

- While going HLE with those EUser functions may help us get away from figuring out the number and implementation of svc call, but also provide functionality between EKA1 and EKA2, the disavantage is to deal with many, many of vtables and typeinfos.

- To get away with this, I built a program which parse all EPOC headers and compile them into a meta.yml, which contains typeinfo of class, funcs and each class vtable entry descriptions.

- The result of this will be used to built vtables stubbing in EKA2L1.
