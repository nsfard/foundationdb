import json
import math
import random
import struct
from collections import OrderedDict
from itertools import chain

Byte = 1
UByte = 2
Bool = 3
Short = 4
UShort = 5
Int = 6
UInt = 7
Float = 8
Long = 9
ULong = 10
Double = 11

# TODO(anoyes): This is becoming unwieldly.

class Type:
    pass


def choose_type(depth, allow_indirection=True, allow_union=True):
    choices = [Primitive]
    if depth > 0:
        choices.append(Struct)
        if allow_indirection:
            choices.extend([Vector, Table])
            if allow_union:
                choices.append(Union)
    return random.choice(choices).choose_type(depth)


def sample_bernoulli(p):
    result = 0
    while True:
        if random.random() > p:
            return result
        result += 1


def sample_string():
    return ''.join(random.choice(
        'abcdefghijklmnopqrstuvwxyz') for _ in range(sample_bernoulli(0.7)))


class Primitive(Type):
    def __init__(self, prim):
        assert(Byte <= prim <= Double)
        self.prim = prim

    def __repr__(self):
        return 'Primitive(%s)' % {
            1: 'Byte',
            2: 'UByte',
            3: 'Bool',
            4: 'Short',
            5: 'UShort',
            6: 'Int',
            7: 'UInt',
            8: 'Float',
            9: 'Long',
            10: 'ULong',
            11: 'Double',
        }[self.prim]

    def to_fbidl(self, tables=None):
        return {
            1: 'byte',
            2: 'ubyte',
            3: 'bool',
            4: 'short',
            5: 'ushort',
            6: 'int',
            7: 'uint',
            8: 'float',
            9: 'long',
            10: 'ulong',
            11: 'double',
            12: 'string',
        }[self.prim]

    def to_cpp(self, tables=None):
        return {
            1: 'int8_t',
            2: 'uint8_t',
            3: 'bool',
            4: 'int16_t',
            5: 'uint16_t',
            6: 'int32_t',
            7: 'uint32_t',
            8: 'float',
            9: 'int64_t',
            10: 'uint64_t',
            11: 'double',
            12: 'std::string',
        }[self.prim]

    def choose_data(self, field):
        b = bytearray(random.getrandbits(8) for _ in range(8))

        def not_nan(c, width, b):
            while True:
                x = struct.unpack(c, b[0:width])[0]
                if not math.isnan(x):
                    return x
                b = bytearray(random.getrandbits(8) for _ in range(8))
        return {field: {
            1: struct.unpack('b', b[0:1])[0],
            2: struct.unpack('B', b[0:1])[0],
            3: random.choice([False, True]),
            4: struct.unpack('h', b[0:2])[0],
            5: struct.unpack('H', b[0:2])[0],
            6: struct.unpack('i', b[0:4])[0],
            7: struct.unpack('I', b[0:4])[0],
            8: not_nan('f', 4, b),
            9: struct.unpack('q', b[0:8])[0],
            10: struct.unpack('Q', b[0:8])[0],
            11: not_nan('d', 8, b),
            12: sample_string(),
        }[self.prim]}

    @staticmethod
    def choose_type(depth=1):
        return Primitive(random.choice(list(range(Byte, Double + 1))))

    def __eq__(self, other):
        return isinstance(other, Primitive) and self.prim == other.prim

    def __hash__(self):
        return hash(self.prim)


class Vector(Type):
    def __init__(self, elem):
        assert(isinstance(elem, Type))
        self.elem = elem

    @staticmethod
    def choose_type(depth):
        return Vector(choose_type(depth - 1, allow_union=False))

    def __repr__(self):
        return 'Vector(%s)' % self.elem.__repr__()

    def to_fbidl(self, tables):
        return '[%s]' % self.elem.to_fbidl(tables)

    def to_cpp(self, tables):
        return 'std::vector<%s>' % self.elem.to_cpp(tables)

    def choose_data(self, field):
        elems = sample_bernoulli(0.9)
        result = []
        for _ in range(elems):
            ds = self.elem.choose_data('x')
            result.append(ds['x'])
        return {field: result}

    def __eq__(self, other):
        return isinstance(other, Vector) and self.elem == other.elem

    def __hash__(self):
        return hash(self.elem)


global_counter = 0
def fresh():
    global global_counter
    result = global_counter
    global_counter += 1
    return result

class Table(Type):
    def __init__(self, fields, name):
        assert(all(isinstance(elem, Type) for (_, elem) in fields.items()))
        assert(all(type(field) == str for (field, _) in fields.items()))
        self.fields = sorted(fields.items(), key=lambda pair: pair[0])
        self.name = name

    @staticmethod
    def choose_type(depth):
        name = 'Table%d' % fresh()
        num_fields = sample_bernoulli(0.9)
        fields = {'m_' + sample_string(): choose_type(
            depth - 1) for _ in range(num_fields)}
        return Table(fields, name)

    def to_fbidl(self, tables):
        return tables.new_table(self.name, self.fields)

    def to_cpp(self, tables):
        return tables.new_table(self.name, self.fields)

    def __repr__(self):
        return 'Table(%s, %s)' % (self.fields.__repr__(), self.name.__repr__())

    def choose_data(self, field):
        result = {}
        for (f, t) in self.fields:
            for (k, v) in t.choose_data(f).items():
                result[k] = v
        return {field: result}

    def __eq__(self, other):
        return isinstance(other, Table) and self.name == other.name

    def __hash__(self):
        return hash(self.name)

class Union(Type):
    def __init__(self, fields_and_types, name):
        assert (all(isinstance(t, Type) for (_, t) in fields_and_types))
        assert (all(type(name) == str for (name, _) in fields_and_types))
        self.fields_and_types = fields_and_types
        self.name = name

    @staticmethod
    def choose_type(depth):
        name = 'Union%d' % fresh()
        num_fields = (min(10, 1 + sample_bernoulli(0.9)) if depth > 0 else random.randint(1, Double))
        names = set()
        types = OrderedDict()
        while len(names) < num_fields:
            names.add('m_' + sample_string())
        while len(types) < num_fields:
            types[Table.choose_type(0)] = None
        return Union(list(zip(sorted(names), types.keys())), name)

    def to_fbidl(self, tables):
        return tables.new_union(self.name, self.fields_and_types)

    def to_cpp(self, tables):
        return 'boost::variant<%s>' % ', '.join(
            (t.to_cpp(tables) for (_, t) in self.fields_and_types))

    def __repr__(self):
        return 'Union(%s, %s)' % (self.fields_and_types.__repr__(), self.name.__repr__())

    def choose_data(self, field):
        i = random.randint(0, len(self.fields_and_types) - 1)
        (_, t) = self.fields_and_types[i]
        assert(isinstance(t, Table))
        result = t.choose_data(field)
        result[field + '_type'] = t.name
        return result

    def __eq__(self, other):
        return isinstance(other, Union) and self.name == other.name

    def __hash__(self):
        return hash(self.name)

class Struct(Type):
    def __init__(self, fields, name):
        assert(all(isinstance(elem, Type) for (_, elem) in fields.items()))
        assert(all(type(field) == str for (field, _) in fields.items()))
        self.fields = sorted(fields.items(), key=lambda pair: pair[0])
        self.name = name

    @staticmethod
    def choose_type(depth):
        name = 'Struct%d' % fresh()
        num_fields = sample_bernoulli(0.9)
        fields = {
            'm_' + sample_string(): choose_type(
                depth - 1, allow_indirection=False)
            for _ in range(num_fields)
        }
        return Struct(fields, name)

    def to_fbidl(self, tables):
        return tables.new_struct(self.name, self.fields)

    def to_cpp(self, tables):
        return 'std::tuple<%s>' % ', '.join(
            (t.to_cpp(tables) for (_, t) in self.fields))

    def __repr__(self):
        return 'Struct(%s, %s)' % (self.fields.__repr__(), self.name.__repr__())

    def choose_data(self, field):
        result = {}
        for (f, t) in self.fields:
            for (k, v) in t.choose_data(f).items():
                result[k] = v
        return {field: result}

    def __eq__(self, other):
        return isinstance(other, Struct) and self.name == other.name

    def __hash__(self):
        return hash(self.name)

class CollectIdlTables:
    tables = []

    def new_struct(self, name, fields):
        struct = 'struct %s {\n' % name
        struct += '\n'.join('    %s:%s;' % (f, t.to_fbidl(self)) for (
            f, t) in fields)
        struct += '\n}\n'
        self.tables.append(struct)
        return name

    def new_table(self, name, fields):
        table = 'table %s {\n' % name
        table += '\n'.join('    %s:%s;' % (f, t.to_fbidl(self)) for (
            f, t) in fields)
        table += '\n}\n'
        self.tables.append(table)
        return name

    def new_union(self, name, fields):
        union = 'union %s {\n' % name
        union += '\n'.join('    %s,' % t.to_fbidl(self) for (
            f, t) in fields)
        union += '\n}\n'
        self.tables.append(union)
        return name



class CollectCppTables:
    tables = []
    counter = 0

    def new_table(self, name, fields):
        self.counter += 1
        table = 'struct %s {\n' % name
        table += '\n'.join('    %s %s = {};' % (t.to_cpp(self), f)
                           for (f, t) in fields) + '\n'
        table += '    template <class Archiver>\n'
        table += '    void serialize(Archiver& ar) {\n'
        table += '        serializer(%s);\n' % ', '.join(
            chain(['ar'], (k for (k, _) in fields)))
        table += '    }\n'
        table += '    friend bool operator==(const ' + name + '& lhs, const ' + name + '& rhs) {\n'
        table += '        return %s;\n' % ' && '.join(
            chain(['true'],
                  ('lhs.{0} == rhs.{0}'.format(k) for (k, _) in fields)))
        table += '    }\n'
        table += '};\n'
        self.tables.append(table)
        return name

    def new_union(self, name, fields):
        return name


def to_fbidl(t):
    collectTables = CollectIdlTables()
    t.to_fbidl(collectTables)
    result = '\n'.join(collectTables.tables)
    result += '\nroot_type Table0;'
    return result


def to_cpp(t):
    collectTables = CollectCppTables()
    t.to_cpp(collectTables)
    result = '#pragma once\n'
    result += '#include <stdint.h>\n'
    result += '#include <vector>\n'
    result += '#include <string>\n'
    result += '\n'
    result += '\n'.join(collectTables.tables)
    return result


if __name__ == "__main__":
    import sys
    random.seed(int(sys.argv[1], 0))
    t = Table.choose_type(2)
    if sys.argv[2] == 'cpp':
        print(to_cpp(t))
    if sys.argv[2] == 'fbs':
        print(to_fbidl(t))
    if sys.argv[2] == 'data':
        random.seed(int(sys.argv[3]))
        result = t.choose_data('x')
        print(json.dumps(result['x']))
