m4_define([REVISION], esyscmd([git rev-parse --short HEAD | tr -d '\n']))
m4_define([BASE_VERSION], [1.8.3])
m4_define([VERSION_NUMBER], BASE_VERSION.REVISION)
