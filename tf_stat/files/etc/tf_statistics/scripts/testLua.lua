
--[[1]]
mt  = {}
function mt.__call(self, a, b, c, d)
	return a..b..c..d
end

foo = setmetatable({}, mt)
foo.key = 'value'

print(foo(10, 20, 30, '!')) --> 102030!
print(foo.key) --> 'value'
print(foo.bar) --> nil


--[[2]]

mt  = {}
-- Многоточие - все аргументы, которые были переданы в функцию
-- например: a, b, c, d = ...
function mt.__call(self, ...)
    return self.default(...)
end

t = {
    a = 1,
    b = 2,
    c = 3
}

foo = setmetatable({}, mt)

function foo.mul2(a, b)
    return a * b
end

function foo.mul3(a, b, c)
    return a * b * c
end

foo.default = foo.mul2

print('foo.mul2(2, 3)', foo.mul2(2, 3)) --> 6
print('foo.default(2, 3)', foo.default(2, 3)) --> 6
print('foo.mul3(2, 3, 4)', foo.mul3(2, 3, 4)) --> 24

-- Вызов значения по умолчанию.
print('foo(2, 3)', foo(2, 3)) --> 6

foo.default = foo.mul3
print('Default was changed')
