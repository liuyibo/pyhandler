def lets_233(s):
    i = 0
    d = 1
    while i < len(s):
        k = 2
        print(s[i:i+k], end='')
        print('233' + d * '3', end='')
        d += 2
        i += k
    print()
