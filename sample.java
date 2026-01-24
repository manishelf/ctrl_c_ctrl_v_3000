package com.example

import com.apache.Logger;

public class HelloWorld extends BusinessLogicBase {
    private int isInt;
    private static final Logger logger = Logger.getLogger(HelloWorld.class);;
    public HelloWorld(){
    }
    public static void main(String[] args) {
        // The line below prints the message to the console
        System.out.println("Hello, World!");
        logger.info("sasas");

        Query hqlQuery = getAm().createQuery("From AbcView as x where x.id = :pqr");
        hqlQuery.setInteger("pqr", 1000);
        List objs = hqlQuery.list();

        NativeQuery query = getAm().createNativeQuery("select count(*) as N from AbcView");
        query.addScalar("N", HibernateType.Integer);
        List objs2 = query.list();
    }
}
